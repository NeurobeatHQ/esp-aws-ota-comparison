/*
 * device_iot.c — the backend-agnostic facade + boot orchestration.
 *
 * Owns the boot sequence (NVS -> Wi-Fi -> transport -> OTA backend -> trial-boot
 * resolution), the single incoming-PUBLISH router (which hands messages to both
 * the OTA backend and the application's subscriptions), and the single transport
 * connection callback (re-subscribes app + OTA topics on reconnect, then notifies
 * the app). The transport and OTA backend are selected at build time.
 */
#include "device_iot.h"
#include "transport.h"
#include "ota_backend.h"
#include "self_test.h"
#include "wifi.h"
#include "app_config.h"

#include <string.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "device_iot";

#ifndef OTA_BACKEND_NAME
#define OTA_BACKEND_NAME "unknown"   /* normally set by src/CMakeLists from the env */
#endif

#define MAX_APP_SUBS 8
#define MAX_FILTER   128
static struct { char filter[MAX_FILTER]; device_iot_msg_cb_t cb; } s_subs[MAX_APP_SUBS];
static int s_sub_count;

static device_iot_conn_cb_t s_conn_cb;
/* gate: don't drive the OTA backend before it starts. volatile — written by the
 * app_main task, read by the transport/event task in on_conn(). */
static volatile bool s_ota_started;
/* True once transport_start() has run. publish/subscribe are no-ops before it, so
 * the "offline / no identity" path (which never starts the transport) can't touch an
 * unstarted send queue. */
static bool s_transport_up;

/* Resolved identity (copied so the caller's config need not outlive init). The
 * Thing name is used for every dt/<thing>/ topic; the endpoint is handed to the
 * transport, which holds it by reference across reconnects. */
static char s_thing_name[MAX_THING_NAME_LEN];
static char s_endpoint[128];

/* MQTT topic-filter match with '+' (single level) and '#' (multi level, last). */
static bool topic_matches(const char *f, const char *t)
{
    while (*f) {
        if (*f == '#') {
            return true;                                   /* matches the remainder */
        }
        if (*f == '/' && f[1] == '#' && *t == '\0') {
            return true;                                   /* "a/#" also matches "a" */
        }
        if (*f == '+') {
            while (*t && *t != '/') t++;                   /* consume one topic level */
            f++;
            if (*f == '\0' && *t == '\0') return true;
            if (*f != *t) return false;                    /* both must be '/' or end */
        } else if (*f != *t) {
            return false;
        }
        if (*f == '\0') return true;
        f++; t++;
    }
    return *t == '\0';                                     /* filter done -> topic must be too */
}

/* The single transport publish callback. Every incoming message goes (1) to the
 * OTA backend (which matches its own reserved/control topics) and (2) to any app
 * subscription whose filter matches the full topic. */
static void on_publish(const char *topic, size_t topic_len,
                       const uint8_t *payload, size_t payload_len)
{
    ota_backend_on_publish(topic, topic_len, payload, payload_len);

    char topic_z[MAX_FILTER];
    size_t tl = topic_len < sizeof(topic_z) - 1 ? topic_len : sizeof(topic_z) - 1;
    memcpy(topic_z, topic, tl);
    topic_z[tl] = '\0';

    for (int i = 0; i < s_sub_count; i++) {
        if (topic_matches(s_subs[i].filter, topic_z)) {
            s_subs[i].cb(topic_z, payload, payload_len);
        }
    }
}

/* The single transport connection callback. On a reconnect the broker has
 * forgotten our (cleanSession) subscriptions, so replay the app's, then let the
 * OTA backend replay its own, then tell the app it is back. */
static void on_conn(bool up)
{
    if (up) {
        for (int i = 0; i < s_sub_count; i++) {
            transport_subscribe(s_subs[i].filter, (uint16_t)strlen(s_subs[i].filter), 1);
        }
        if (s_ota_started) {
            ota_backend_on_reconnect();   /* skipped on the first connect during init */
        }
        if (s_conn_cb) s_conn_cb(true);
    } else {
        if (s_conn_cb) s_conn_cb(false);
    }
}

/* The server root CA is public and always embedded (embed_certs.cmake). The device
 * cert + key are NOT embedded — they live in the esp_secure_cert partition, flashed
 * per device by scripts/provision-secure-cert.sh, so one image serves every board and
 * the private key is never in the .bin. */
extern const unsigned char aws_root_ca_pem[];

#include "esp_secure_cert_read.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/oid.h"

/* BYO-CA device certs are issued with CN=<thing-name>, so the Thing name (= the MQTT
 * clientId) is the cert's subject CN. Parsing it here lets ONE firmware image serve
 * the whole fleet: identity comes from the provisioned cert in esp_secure_cert, not a
 * compiled-in macro. Returns NULL if the cert can't be parsed or has no CN. */
static char s_cn[MAX_THING_NAME_LEN];
static const char *thing_name_from_cert(const char *cert_pem)
{
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    const char *out = NULL;
    /* PEM parse needs the length to include the NUL; the cert buffer is a C string. */
    if (mbedtls_x509_crt_parse(&crt, (const unsigned char *)cert_pem, strlen(cert_pem) + 1) == 0) {
        for (const mbedtls_x509_name *n = &crt.subject; n != NULL; n = n->next) {
            if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &n->oid) == 0) {
                size_t len = n->val.len < sizeof(s_cn) - 1 ? n->val.len : sizeof(s_cn) - 1;
                memcpy(s_cn, n->val.p, len);
                s_cn[len] = '\0';
                out = s_cn;
                break;
            }
        }
    }
    mbedtls_x509_crt_free(&crt);
    return out;
}

esp_err_t device_iot_default_config(device_iot_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->endpoint    = AWS_IOT_ENDPOINT;
    cfg->port        = AWS_MQTT_PORT;
    cfg->root_ca_pem = (const char *)aws_root_ca_pem;   /* public; stays embedded */
    /* thing_name is left NULL here and resolved from the cert CN below. */

    /* Device cert. For a cust_flash_tlv partition the buffer is flash-mapped and
     * persists, so the transport may hold it by reference for the connection's life. */
    char *cert = NULL;
    uint32_t cert_len = 0;
    esp_err_t err = esp_secure_cert_get_device_cert(&cert, &cert_len);
    if (err != ESP_OK || cert == NULL) {
        ESP_LOGE(TAG, "esp_secure_cert: no device cert (provision the partition?): %s",
                 esp_err_to_name(err));
        return err != ESP_OK ? err : ESP_ERR_NOT_FOUND;
    }
    cfg->client_cert_pem = cert;

    /* Thing name (= clientId) is the cert's CN with BYO-CA certs (issued CN=<thing>).
     * One image, whole fleet; no compiled-in name. No CN -> no identity (else branch). */
    const char *cn = thing_name_from_cert(cert);
    if (cn != NULL) {
        cfg->thing_name = cn;
        ESP_LOGI(TAG, "esp_secure_cert: Thing name from cert CN = '%s'", cn);
    } else {
        /* No CN -> no device identity. Do NOT fall back to the compiled placeholder:
         * with one image across the fleet, every mis-provisioned board would collide on
         * it. Leave thing_name NULL and signal it distinctly; device_iot_init runs
         * offline on a normal boot and rolls back on a trial boot. */
        cfg->thing_name = NULL;
        ESP_LOGE(TAG, "esp_secure_cert: device cert has no CN -> no device identity");
    }

    /* Private key. DS-peripheral vs plaintext is a build-time choice in
     * esp_secure_cert_mgr (CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL) — the two expose
     * different read APIs, so the firmware must be built to match how the partition
     * was provisioned (provision-secure-cert.sh, with or without --ds). */
#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
    esp_ds_data_ctx_t *ds_ctx = esp_secure_cert_get_ds_ctx();
    if (ds_ctx == NULL) {
        ESP_LOGE(TAG, "esp_secure_cert: DS build but no DS context (provision with --ds?)");
        return ESP_ERR_NOT_FOUND;
    }
    cfg->use_secure_element = true;
    cfg->ds_data            = ds_ctx;
    cfg->client_key_pem     = NULL;
    ESP_LOGI(TAG, "esp_secure_cert: device key in DS peripheral");
#else
    char *key = NULL;
    uint32_t key_len = 0;
    err = esp_secure_cert_get_priv_key(&key, &key_len);
    if (err != ESP_OK || key == NULL) {
        ESP_LOGE(TAG, "esp_secure_cert: no private key: %s", esp_err_to_name(err));
        return err != ESP_OK ? err : ESP_ERR_NOT_FOUND;
    }
    cfg->client_key_pem = key;
    ESP_LOGI(TAG, "esp_secure_cert: device key plaintext in partition");
#endif
    /* Cert + key are valid; the only failure left is a cert with no CN (no identity),
     * flagged distinctly. app_main tolerates it; device_iot_init runs offline / rolls back. */
    return (cfg->thing_name != NULL) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

/* Poll for cloud connectivity up to a budget. Used only as the trial-boot gate;
 * resolve_trial_boot then reads transport_is_connected() one-shot. */
static bool wait_cloud_connected(uint32_t timeout_ms)
{
    for (uint32_t waited = 0; waited < timeout_ms; waited += 200) {
        if (transport_is_connected()) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    /* One final one-shot check: catch a connection that came up during the last
     * ~200 ms sleep, so a good image isn't rolled back at the timeout boundary. */
    return transport_is_connected();
}

esp_err_t device_iot_init(const device_iot_config_t *cfg)
{
    if (cfg == NULL || cfg->endpoint == NULL ||
        cfg->root_ca_pem == NULL || cfg->client_cert_pem == NULL ||
        (!cfg->use_secure_element && cfg->client_key_pem == NULL)) {
        ESP_LOGE(TAG, "device_iot_init: incomplete config (use device_iot_default_config())");
        return ESP_ERR_INVALID_ARG;
    }
    /* thing_name is checked below, after the trial decision — a NULL/empty name is the
     * "no device identity" case, handled specially (offline on a normal boot, rollback
     * on a trial boot) rather than rejected as a bad config. */
    snprintf(s_endpoint, sizeof(s_endpoint), "%s", cfg->endpoint);

    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    self_test_log_boot_info();

    bool trial = self_test_is_trial_boot();
    if (trial) {
        self_test_arm_watchdog();   /* anti-brick guard before the long bring-up */
    }

    /* No usable device identity (e.g. an esp_secure_cert cert with no CN). Never
     * impersonate a placeholder — in the one-image fleet that would collide with a real
     * board's clientId. A trial image must ROLL BACK so a mis-provisioned OTA can't
     * stick; a normal boot runs OFFLINE: Wi-Fi + the app stay up (HTTP/web still works),
     * but no MQTT/cloud link and no OTA backend are started. */
    if (cfg->thing_name == NULL || cfg->thing_name[0] == '\0') {
        if (trial) {
            ESP_LOGE(TAG, "no device identity on a trial image -> rejecting (rollback)");
            self_test_reject_and_rollback();   /* does not return */
        }
        ESP_LOGE(TAG, "no device identity -> OFFLINE: Wi-Fi + app run, no MQTT/OTA");
        wifi_start();   /* stay on the network; just never open the AWS IoT MQTT link */
        return ESP_OK;
    }
    snprintf(s_thing_name, sizeof(s_thing_name), "%s", cfg->thing_name);

    wifi_start();   /* non-blocking; keeps (re)connecting in the background */

    if (trial) {
        /* The trial image must prove it can reach its network — block hard, and if
         * it can't connect within the budget, reject so the bootloader rolls back. */
        if (wifi_wait_connected(WIFI_TRIAL_CONNECT_TIMEOUT_MS) != ESP_OK) {
            ESP_LOGE(TAG, "no Wi-Fi on trial image -> rolling back");
            self_test_reject_and_rollback();   /* does not return */
        }
        self_test_feed_watchdog();
    } else {
        /* Normal boot: the app runs with or without Wi-Fi. Wait briefly so a present
         * network is up before the first publish, then carry on regardless — Wi-Fi
         * and the transport keep reconnecting in the background. Never block hard. */
        if (wifi_wait_connected(WIFI_CONNECT_TIMEOUT_MS) != ESP_OK) {
            ESP_LOGW(TAG, "no Wi-Fi; running app offline (retrying in background)");
        }
    }

    transport_set_conn_cb(on_conn);             /* device_iot owns the single slot */
    transport_config_t tcfg = {
        .endpoint   = s_endpoint,        /* copied above */
        .port       = cfg->port,
        .thing_name = s_thing_name,      /* copied above */
        .root_ca_pem        = cfg->root_ca_pem,
        .client_cert_pem    = cfg->client_cert_pem,
        .client_key_pem     = cfg->client_key_pem,
        .use_secure_element = cfg->use_secure_element,
        .ds_data            = cfg->ds_data,
    };
    transport_start(on_publish, &tcfg);         /* non-blocking; connects in background */
    s_transport_up = true;                      /* publish/subscribe are live from here */

    if (trial) {
        /* Cloud reachability is the other half of the trial gate: give the transport
         * a budget to connect, then resolve_trial_boot reads it one-shot (commit vs
         * rollback). A normal boot doesn't wait — the app starts immediately. */
        if (!wait_cloud_connected(CLOUD_TRIAL_CONNECT_TIMEOUT_MS)) {
            ESP_LOGW(TAG, "trial: cloud not up within budget -> self-test will roll back");
        }
        self_test_feed_watchdog();
    }

    ota_backend_start();   /* resolves the trial boot + services OTA in the background */
    s_ota_started = true;  /* on_conn may now drive ota_backend_on_reconnect() */

    /* First job-topic subscribe + StartNext go through the SAME path as a reconnect,
     * exactly once: if already connected (e.g. a trial boot that waited for the cloud
     * above), do it now; otherwise on_conn fires it when the link comes up. */
    if (transport_is_connected()) {
        ota_backend_on_reconnect();
    }

    ESP_LOGI(TAG, "up — backend '%s', firmware v%d.%d.%d", OTA_BACKEND_NAME,
             APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);

    /* First-connect and every reconnect are announced by the transport via on_conn,
     * so the app's connection cb + birth fire regardless of how slow the first
     * connect is (the gap karen found in the old init-tail one-shot). */
    return ESP_OK;
}

bool device_iot_is_connected(void)
{
    return transport_is_connected();
}

esp_err_t device_iot_publish_topic(const char *topic, const void *data, size_t len, int qos)
{
    if (!s_transport_up) return ESP_ERR_INVALID_STATE;   /* offline (no identity): no transport */
    return transport_publish(topic, (uint16_t)strlen(topic), data, len, (uint8_t)qos);
}

esp_err_t device_iot_publish(const char *subtopic, const void *data, size_t len, int qos)
{
    if (!s_transport_up) return ESP_ERR_INVALID_STATE;   /* offline (no identity): no transport */
    char topic[MAX_FILTER];
    int n = snprintf(topic, sizeof(topic), "dt/%s/%s", s_thing_name, subtopic);
    if (n <= 0 || n >= (int)sizeof(topic)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return transport_publish(topic, (uint16_t)n, data, len, (uint8_t)qos);
}

esp_err_t device_iot_subscribe_topic(const char *filter, device_iot_msg_cb_t cb)
{
    if (!s_transport_up) return ESP_ERR_INVALID_STATE;   /* offline (no identity): no transport */
    if (s_sub_count >= MAX_APP_SUBS || strlen(filter) >= MAX_FILTER) {
        return ESP_ERR_NO_MEM;
    }
    strncpy(s_subs[s_sub_count].filter, filter, MAX_FILTER - 1);
    s_subs[s_sub_count].cb = cb;
    s_sub_count++;
    return transport_subscribe(filter, (uint16_t)strlen(filter), 1);
}

esp_err_t device_iot_subscribe(const char *subtopic, device_iot_msg_cb_t cb)
{
    char filter[MAX_FILTER];
    int n = snprintf(filter, sizeof(filter), "dt/%s/%s", s_thing_name, subtopic);
    if (n <= 0 || n >= (int)sizeof(filter)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return device_iot_subscribe_topic(filter, cb);
}

void device_iot_set_connection_cb(device_iot_conn_cb_t cb)
{
    s_conn_cb = cb;
}

void device_iot_set_health_check(device_iot_health_cb_t cb)
{
    self_test_set_health_cb(cb);   /* the backend calls this during a trial boot */
}

const char *device_iot_backend_name(void)
{
    return OTA_BACKEND_NAME;
}

const char *device_iot_thing_name(void)
{
    return s_thing_name;
}

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

esp_err_t device_iot_init(const device_iot_config_t *cfg)
{
    /* Resolve identity: each field falls back to the build-time default, so
     * device_iot_init(NULL) is exactly the old compile-time behavior. */
    snprintf(s_thing_name, sizeof(s_thing_name), "%s",
             (cfg && cfg->thing_name) ? cfg->thing_name : THING_NAME);
    snprintf(s_endpoint, sizeof(s_endpoint), "%s",
             (cfg && cfg->endpoint) ? cfg->endpoint : AWS_IOT_ENDPOINT);

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

    if (wifi_connect_blocking() != ESP_OK) {
        if (trial) {
            ESP_LOGE(TAG, "no Wi-Fi on trial image -> rolling back");
            self_test_reject_and_rollback();   /* does not return */
        }
        ESP_LOGE(TAG, "no Wi-Fi; restarting in 5s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    if (trial) self_test_feed_watchdog();

    transport_set_conn_cb(on_conn);             /* device_iot owns the single slot */
    transport_config_t tcfg = {
        .endpoint   = s_endpoint,
        .port       = (cfg && cfg->port) ? cfg->port : AWS_MQTT_PORT,
        .thing_name = s_thing_name,
        .root_ca_pem        = cfg ? cfg->root_ca_pem        : NULL,
        .client_cert_pem    = cfg ? cfg->client_cert_pem    : NULL,
        .client_key_pem     = cfg ? cfg->client_key_pem     : NULL,
        .use_secure_element = cfg ? cfg->use_secure_element : false,
        .ds_data            = cfg ? cfg->ds_data            : NULL,
    };
    if (transport_start(on_publish, &tcfg) != ESP_OK) {
        ESP_LOGW(TAG, "initial cloud connect pending (backend keeps retrying)");
    }
    if (trial) self_test_feed_watchdog();

    ota_backend_start();   /* resolves the trial boot + services OTA in the background */
    s_ota_started = true;  /* on_conn may now drive ota_backend_on_reconnect() */

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
    return transport_publish(topic, (uint16_t)strlen(topic), data, len, (uint8_t)qos);
}

esp_err_t device_iot_publish(const char *subtopic, const void *data, size_t len, int qos)
{
    char topic[MAX_FILTER];
    int n = snprintf(topic, sizeof(topic), "dt/%s/%s", s_thing_name, subtopic);
    if (n <= 0 || n >= (int)sizeof(topic)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return transport_publish(topic, (uint16_t)n, data, len, (uint8_t)qos);
}

esp_err_t device_iot_subscribe_topic(const char *filter, device_iot_msg_cb_t cb)
{
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

/* transport_espmqtt.c — esp-mqtt mutual-TLS transport to AWS IoT Core
 * (used by the jobs + manual backends). */
#include "transport.h"
#include "app_config.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "mqtt_client.h"     /* esp-mqtt (IDF component "mqtt") */

static const char *TAG = "mqtt_es";

/* The TLS identity (endpoint, certs/key) comes from the resolved transport_config_t;
 * device_iot supplies the device cert/key from the esp_secure_cert partition (plus the
 * embedded public root CA) via device_iot_default_config(). */

static esp_mqtt_client_handle_t s_client;
static volatile bool s_connected;
static transport_publish_cb_t s_publish_cb;
static transport_conn_cb_t s_conn_cb;
static transport_config_t s_cfg;        /* resolved endpoint/thing/certs (held by ref) */
static unsigned s_connect_count;

/* Reassembly of chunked MQTT_EVENT_DATA (esp-mqtt splits payloads > buffer). */
static char    s_rx_topic[256];
static size_t  s_rx_topic_len;
static uint8_t s_rx_buf[MQTT_NETWORK_BUFFER_SIZE];
static size_t  s_rx_total;

static void handle_data(esp_mqtt_event_handle_t e)
{
    /* First fragment of a message carries the topic (topic_len > 0). */
    if (e->topic != NULL && e->topic_len > 0) {
        s_rx_topic_len = e->topic_len < sizeof(s_rx_topic) ? e->topic_len : sizeof(s_rx_topic) - 1;
        memcpy(s_rx_topic, e->topic, s_rx_topic_len);
        s_rx_topic[s_rx_topic_len] = '\0';   /* NUL-terminate for the callback */
        s_rx_total = e->total_data_len;
        if (s_rx_total > sizeof(s_rx_buf)) {
            ESP_LOGW(TAG, "message %u > buffer %u; truncating",
                     (unsigned)s_rx_total, (unsigned)sizeof(s_rx_buf));
            s_rx_total = sizeof(s_rx_buf);
        }
    }
    /* Copy this fragment at its offset. */
    if (e->current_data_offset + e->data_len <= sizeof(s_rx_buf)) {
        memcpy(s_rx_buf + e->current_data_offset, e->data, e->data_len);
    }
    /* Deliver once the whole payload is in. */
    if (e->current_data_offset + e->data_len >= s_rx_total) {
        if (s_publish_cb != NULL) {
            s_publish_cb(s_rx_topic, s_rx_topic_len, s_rx_buf, s_rx_total);
        }
    }
}

static void mqtt_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)data;
    switch ((esp_mqtt_event_id_t)id) {
        case MQTT_EVENT_CONNECTED:
            s_connected = true;
            if (++s_connect_count > 1) ESP_LOGW(TAG, "reconnected");
            else ESP_LOGI(TAG, "MQTT connected to AWS IoT Core as '%s'", s_cfg.thing_name);
            /* Fire on EVERY connect incl. the first — device_iot is idempotent. */
            if (s_conn_cb != NULL) s_conn_cb(true);
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_connected = false;
            ESP_LOGW(TAG, "MQTT disconnected");
            if (s_conn_cb != NULL) s_conn_cb(false);
            break;
        case MQTT_EVENT_DATA:
            handle_data(e);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;
        default:
            break;
    }
}

esp_err_t transport_start(transport_publish_cb_t cb, const transport_config_t *cfg)
{
    s_publish_cb = cb;
    s_cfg = *cfg;        /* shallow copy: endpoint/thing_name/PEM held by reference */

    static char uri[160];
    snprintf(uri, sizeof(uri), "mqtts://%s:%d", s_cfg.endpoint, s_cfg.port);

    /* Certs come straight from the config (NUL-terminated, so esp-mqtt takes them
     * by string and computes the length itself). */
    esp_mqtt_client_config_t mcfg = {
        .broker = {
            .address.uri = uri,
            .verification.certificate = s_cfg.root_ca_pem,               /* server root CA */
        },
        .credentials = {
            .client_id = s_cfg.thing_name,                               /* clientId == Thing */
            .authentication = { .certificate = s_cfg.client_cert_pem },  /* mutual TLS */
        },
        .session.keepalive = MQTT_KEEP_ALIVE_SECONDS,
        .buffer.size = MQTT_NETWORK_BUFFER_SIZE,
        /* Bound the outbox so a QoS1 backlog on a slow link can't grow until OOM;
         * once full, esp_mqtt_client_enqueue returns -2 -> ESP_ERR_NO_MEM. */
        .outbox.limit = MQTT_OUTBOX_LIMIT_BYTES,
    };
    if (s_cfg.use_secure_element) {
        mcfg.credentials.authentication.ds_data = s_cfg.ds_data;         /* key in DS peripheral */
    } else {
        mcfg.credentials.authentication.key = s_cfg.client_key_pem;
    }

    s_client = esp_mqtt_client_init(&mcfg);
    if (s_client == NULL) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler, NULL));
    /* NON-BLOCKING: esp-mqtt connects (and reconnects) in its own task and fires the
     * conn cb on MQTT_EVENT_CONNECTED. device_iot decides whether to wait (trial
     * gate); the transport never blocks the boot. */
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));
    return ESP_OK;
}

void transport_set_conn_cb(transport_conn_cb_t cb) { s_conn_cb = cb; }
bool transport_is_connected(void) { return s_connected; }

esp_err_t transport_publish(const char *topic, uint16_t topic_len,
                          const void *payload, size_t payload_len, uint8_t qos)
{
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    char topic_z[256];
    if (topic_len >= sizeof(topic_z)) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(topic_z, topic, topic_len);
    topic_z[topic_len] = '\0';

    /* enqueue (store=true): non-blocking, goes to the bounded outbox and is sent
     * by the mqtt task; QoS1/2 are retained there for retransmit until PUBACK'd. */
    int id = esp_mqtt_client_enqueue(s_client, topic_z, (const char *)payload,
                                     (int)payload_len, qos, 0, true);
    if (id == -2) return ESP_ERR_NO_MEM;      /* outbox full -> uniform backpressure */
    return (id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t transport_subscribe(const char *topic_filter, uint16_t filter_len, uint8_t qos)
{
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    char filter_z[256];
    if (filter_len >= sizeof(filter_z)) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(filter_z, topic_filter, filter_len);
    filter_z[filter_len] = '\0';

    int id = esp_mqtt_client_subscribe_single(s_client, filter_z, qos);
    if (id < 0) {
        ESP_LOGE(TAG, "subscribe to %s failed", filter_z);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "subscribed: %s", filter_z);
    return ESP_OK;
}

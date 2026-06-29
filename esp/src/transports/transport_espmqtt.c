/* mqtt_es.c — esp-mqtt mutual-TLS wrapper to AWS IoT Core (variants A + B). */
#include "transport.h"
#include "app_config.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "mqtt_client.h"     /* esp-mqtt (IDF component "mqtt") */

static const char *TAG = "mqtt_es";

/* Certificates embedded as generated C arrays (see embed_certs.cmake).
 * NUL-terminated, so esp-mqtt is given length 0 (it uses strlen). */
extern const unsigned char aws_root_ca_pem[];
extern const unsigned char device_cert_pem[];
extern const unsigned char device_key_pem[];

#define CONNECTED_BIT  BIT0

static esp_mqtt_client_handle_t s_client;
static volatile bool s_connected;
static transport_publish_cb_t s_publish_cb;
static transport_conn_cb_t s_conn_cb;
static transport_config_t s_cfg;        /* resolved endpoint/thing/certs (held by ref) */
static EventGroupHandle_t s_events;
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
            xEventGroupSetBits(s_events, CONNECTED_BIT);
            if (++s_connect_count > 1) ESP_LOGW(TAG, "reconnected");
            else ESP_LOGI(TAG, "MQTT connected to AWS IoT Core as '%s'", s_cfg.thing_name);
            /* Fire on EVERY connect incl. the first — even if the initial connect
             * lands after transport_start's wait window (device_iot is idempotent). */
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
    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        return ESP_ERR_NO_MEM;
    }

    static char uri[160];
    snprintf(uri, sizeof(uri), "mqtts://%s:%d", s_cfg.endpoint, s_cfg.port);

    /* Certs from the config, else the build-embedded arrays (all NUL-terminated,
     * so esp-mqtt takes them by string and computes the length itself). */
    const char *root = s_cfg.root_ca_pem     ? s_cfg.root_ca_pem     : (const char *)aws_root_ca_pem;
    const char *cert = s_cfg.client_cert_pem ? s_cfg.client_cert_pem : (const char *)device_cert_pem;
    const char *key  = s_cfg.client_key_pem  ? s_cfg.client_key_pem  : (const char *)device_key_pem;

    esp_mqtt_client_config_t mcfg = {
        .broker = {
            .address.uri = uri,
            .verification.certificate = root,                            /* server root CA */
        },
        .credentials = {
            .client_id = s_cfg.thing_name,                               /* clientId == Thing */
            .authentication = { .certificate = cert },                   /* mutual TLS */
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
        mcfg.credentials.authentication.key = key;
    }

    s_client = esp_mqtt_client_init(&mcfg);
    if (s_client == NULL) {
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));

    EventBits_t bits = xEventGroupWaitBits(s_events, CONNECTED_BIT, pdFALSE, pdTRUE,
                                           pdMS_TO_TICKS(MQTT_INITIAL_CONNECT_TIMEOUT_MS));
    return (bits & CONNECTED_BIT) ? ESP_OK : ESP_FAIL;   /* keeps retrying either way */
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

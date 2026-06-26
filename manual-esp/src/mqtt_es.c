/* mqtt_es.c — esp-mqtt mutual-TLS wrapper to AWS IoT Core (variants A + B). */
#include "mqtt_es.h"
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
static mqtt_es_publish_cb_t s_publish_cb;
static mqtt_es_reconnect_cb_t s_reconnect_cb;
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
            if (++s_connect_count > 1 && s_reconnect_cb != NULL) {
                ESP_LOGW(TAG, "reconnected");
                s_reconnect_cb();
            } else {
                ESP_LOGI(TAG, "MQTT connected to AWS IoT Core as '%s'", THING_NAME);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            s_connected = false;
            ESP_LOGW(TAG, "MQTT disconnected");
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

esp_err_t mqtt_es_start(mqtt_es_publish_cb_t cb)
{
    s_publish_cb = cb;
    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        return ESP_ERR_NO_MEM;
    }

    static char uri[160];
    snprintf(uri, sizeof(uri), "mqtts://%s:%d", AWS_IOT_ENDPOINT, AWS_MQTT_PORT);

    esp_mqtt_client_config_t cfg = {
        .broker = {
            .address.uri = uri,
            .verification.certificate = (const char *)aws_root_ca_pem,   /* server root CA */
        },
        .credentials = {
            .client_id = THING_NAME,                                     /* clientId == Thing */
            .authentication = {
                .certificate = (const char *)device_cert_pem,            /* mutual TLS */
                .key = (const char *)device_key_pem,
            },
        },
        .session.keepalive = MQTT_KEEP_ALIVE_SECONDS,
        .buffer.size = MQTT_NETWORK_BUFFER_SIZE,
    };

    s_client = esp_mqtt_client_init(&cfg);
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

void mqtt_es_set_reconnect_cb(mqtt_es_reconnect_cb_t cb) { s_reconnect_cb = cb; }
bool mqtt_es_is_connected(void) { return s_connected; }

esp_err_t mqtt_es_publish(const char *topic, uint16_t topic_len,
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

    int id = esp_mqtt_client_publish(s_client, topic_z, (const char *)payload,
                                     (int)payload_len, qos, 0);
    return (id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_es_subscribe(const char *topic_filter, uint16_t filter_len, uint8_t qos)
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

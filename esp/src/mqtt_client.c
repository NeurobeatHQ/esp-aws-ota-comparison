/* mqtt_client.c — coreMQTT over mutual TLS to AWS IoT Core. */
#include "mqtt_client.h"
#include "app_config.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "core_mqtt.h"
#include "network_transport.h"      /* esp-tls transport from coreMQTT port */
#include "backoff_algorithm.h"

static const char *TAG = "mqtt";

/* Certificates embedded as generated C arrays (see embed_certs.cmake).
 * Lengths include the trailing NUL, which esp-tls expects for PEM material. */
extern const unsigned char aws_root_ca_pem[];   extern const unsigned int aws_root_ca_pem_len;
extern const unsigned char device_cert_pem[];   extern const unsigned int device_cert_pem_len;
extern const unsigned char device_key_pem[];    extern const unsigned int device_key_pem_len;

static MQTTContext_t s_mqtt_ctx;
static NetworkContext_t s_net_ctx;
static TransportInterface_t s_transport;
static uint8_t s_mqtt_buffer[MQTT_NETWORK_BUFFER_SIZE];
static SemaphoreHandle_t s_mqtt_mutex;          /* serialises s_mqtt_ctx access */
static volatile bool s_connected;
static mqtt_incoming_publish_cb_t s_publish_cb;
static mqtt_reconnect_cb_t s_reconnect_cb;

#define MQTT_LOCK()    xSemaphoreTakeRecursive(s_mqtt_mutex, portMAX_DELAY)
#define MQTT_UNLOCK()  xSemaphoreGiveRecursive(s_mqtt_mutex)

#define CONNACK_TIMEOUT_MS         5000U
#define RECONNECT_BACKOFF_BASE_MS  1000U
#define RECONNECT_BACKOFF_MAX_MS   16000U
#define RECONNECT_FOREVER          0U      /* 0 = retry forever (background task) */

static uint32_t getTimeMs(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void mqttEventCallback(MQTTContext_t *pCtx,
                              MQTTPacketInfo_t *pPacketInfo,
                              MQTTDeserializedInfo_t *pDeserializedInfo)
{
    (void)pCtx;
    if ((pPacketInfo->type & 0xF0U) == MQTT_PACKET_TYPE_PUBLISH) {
        MQTTPublishInfo_t *pub = pDeserializedInfo->pPublishInfo;
        if (s_publish_cb != NULL && pub != NULL) {
            s_publish_cb(pub->pTopicName, pub->topicNameLength,
                         (const uint8_t *)pub->pPayload, pub->payloadLength);
        }
    }
    /* SUBACK / PUBACK / PINGRESP: nothing to do for this POC's QoS-0 traffic. */
}

static void fill_network_context(void)
{
    memset(&s_net_ctx, 0, sizeof(s_net_ctx));
    s_net_ctx.pcHostname       = AWS_IOT_ENDPOINT;
    s_net_ctx.xPort            = AWS_MQTT_PORT;
    s_net_ctx.pcServerRootCA   = (const char *)aws_root_ca_pem;
    s_net_ctx.pcServerRootCASize = aws_root_ca_pem_len;            /* incl. NUL */
    s_net_ctx.pcClientCert     = (const char *)device_cert_pem;
    s_net_ctx.pcClientCertSize = device_cert_pem_len;
    s_net_ctx.pcClientKey      = (const char *)device_key_pem;
    s_net_ctx.pcClientKeySize  = device_key_pem_len;
    s_net_ctx.disableSni       = pdFALSE;
    /* Plaintext key for the POC. The DS-peripheral / esp_secure_cert swap is just:
     *   s_net_ctx.use_secure_element = true;  s_net_ctx.ds_data = <ctx>;
     * no protocol change required. */
    s_net_ctx.use_secure_element = false;
    s_net_ctx.ds_data = NULL;
    if (s_net_ctx.xTlsContextSemaphore == NULL) {
        s_net_ctx.xTlsContextSemaphore = xSemaphoreCreateMutex();
    }
}

static esp_err_t mqtt_establish(void)
{
    /* Short TLS recv timeout so MQTT_ProcessLoop (which holds the context mutex)
     * returns quickly when idle, letting the OTA task publish block requests
     * promptly. The 2 s default would throttle the download to ~1 block / 2 s. */
    vTlsSetRecvTimeout(100);

    if (xTlsConnect(&s_net_ctx) != TLS_TRANSPORT_SUCCESS) {
        ESP_LOGE(TAG, "TLS connect to %s:%d failed", AWS_IOT_ENDPOINT, AWS_MQTT_PORT);
        return ESP_FAIL;
    }

    s_transport.pNetworkContext = &s_net_ctx;
    s_transport.send = espTlsTransportSend;
    s_transport.recv = espTlsTransportRecv;
    s_transport.writev = NULL;

    MQTTFixedBuffer_t fixedBuffer = { .pBuffer = s_mqtt_buffer, .size = sizeof(s_mqtt_buffer) };
    if (MQTT_Init(&s_mqtt_ctx, &s_transport, getTimeMs, mqttEventCallback, &fixedBuffer) != MQTTSuccess) {
        ESP_LOGE(TAG, "MQTT_Init failed");
        xTlsDisconnect(&s_net_ctx);
        return ESP_FAIL;
    }

    MQTTConnectInfo_t connectInfo = { 0 };
    connectInfo.cleanSession = true;
    connectInfo.pClientIdentifier = THING_NAME;
    connectInfo.clientIdentifierLength = (uint16_t)strlen(THING_NAME);
    connectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE_SECONDS;

    bool sessionPresent = false;
    MQTTStatus_t st = MQTT_Connect(&s_mqtt_ctx, &connectInfo, NULL,
                                   CONNACK_TIMEOUT_MS, &sessionPresent);
    if (st != MQTTSuccess) {
        ESP_LOGE(TAG, "MQTT_Connect failed: %s", MQTT_Status_strerror(st));
        xTlsDisconnect(&s_net_ctx);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MQTT connected to AWS IoT Core as '%s'", THING_NAME);
    return ESP_OK;
}

static esp_err_t mqtt_connect_with_backoff(uint32_t max_attempts)
{
    BackoffAlgorithmContext_t backoff;
    BackoffAlgorithmStatus_t bo = BackoffAlgorithmSuccess;
    uint16_t nextDelayMs = 0;

    fill_network_context();
    BackoffAlgorithm_InitializeParams(&backoff, RECONNECT_BACKOFF_BASE_MS,
                                      RECONNECT_BACKOFF_MAX_MS, max_attempts);

    for (;;) {
        if (mqtt_establish() == ESP_OK) {
            return ESP_OK;
        }
        /* getTimeMs() low byte gives us a little jitter without rand(). */
        bo = BackoffAlgorithm_GetNextBackoff(&backoff, getTimeMs(), &nextDelayMs);
        if (bo == BackoffAlgorithmRetriesExhausted) {
            return ESP_FAIL;
        }
        ESP_LOGW(TAG, "retrying connection in %u ms", nextDelayMs);
        vTaskDelay(pdMS_TO_TICKS(nextDelayMs));
    }
}

static void mqtt_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (!s_connected) {
            if (mqtt_connect_with_backoff(RECONNECT_FOREVER) == ESP_OK) {
                s_connected = true;
                if (s_reconnect_cb != NULL) {
                    s_reconnect_cb();   /* re-subscribe after a cleanSession reconnect */
                }
            } else {
                vTaskDelay(pdMS_TO_TICKS(RECONNECT_BACKOFF_MAX_MS));
                continue;
            }
        }

        MQTT_LOCK();
        MQTTStatus_t st = MQTT_ProcessLoop(&s_mqtt_ctx);
        MQTT_UNLOCK();

        if (st != MQTTSuccess && st != MQTTNeedMoreBytes) {
            ESP_LOGE(TAG, "process loop error: %s -> reconnecting", MQTT_Status_strerror(st));
            s_connected = false;
            xTlsDisconnect(&s_net_ctx);
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t mqtt_client_start(mqtt_incoming_publish_cb_t cb)
{
    s_publish_cb = cb;
    s_mqtt_mutex = xSemaphoreCreateRecursiveMutex();
    if (s_mqtt_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* Bounded initial connect: on a trial boot, failing fast lets the self-test
     * roll back promptly. The background task then reconnects forever. */
    esp_err_t initial = mqtt_connect_with_backoff(MQTT_INITIAL_CONNECT_ATTEMPTS);
    s_connected = (initial == ESP_OK);

    /* Generous stack: the reconnect path runs the mbedTLS handshake here. */
    if (xTaskCreate(mqtt_task, "mqtt", 8192, NULL, 5, NULL) != pdPASS) {
        return ESP_FAIL;
    }
    return initial;   /* ESP_OK if connected; caller treats !connected as no cloud yet */
}

void mqtt_client_set_reconnect_cb(mqtt_reconnect_cb_t cb)
{
    s_reconnect_cb = cb;
}

bool mqtt_client_is_connected(void)
{
    return s_connected;
}

esp_err_t mqtt_client_publish(const char *topic, uint16_t topic_len,
                              const void *payload, size_t payload_len, uint8_t qos)
{
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    MQTTPublishInfo_t info = { 0 };
    info.qos = (MQTTQoS_t)qos;
    info.pTopicName = topic;
    info.topicNameLength = topic_len;
    info.pPayload = payload;
    info.payloadLength = payload_len;

    MQTT_LOCK();
    uint16_t packetId = (qos == 0) ? 0 : MQTT_GetPacketId(&s_mqtt_ctx);
    MQTTStatus_t st = MQTT_Publish(&s_mqtt_ctx, &info, packetId);
    MQTT_UNLOCK();

    if (st != MQTTSuccess) {
        ESP_LOGE(TAG, "publish to %.*s failed: %s", topic_len, topic, MQTT_Status_strerror(st));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t mqtt_client_subscribe(const char *topic_filter, uint16_t filter_len, uint8_t qos)
{
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    MQTTSubscribeInfo_t sub = { 0 };
    sub.qos = (MQTTQoS_t)qos;
    sub.pTopicFilter = topic_filter;
    sub.topicFilterLength = filter_len;

    MQTT_LOCK();
    MQTTStatus_t st = MQTT_Subscribe(&s_mqtt_ctx, &sub, 1, MQTT_GetPacketId(&s_mqtt_ctx));
    MQTT_UNLOCK();

    if (st != MQTTSuccess) {
        ESP_LOGE(TAG, "subscribe to %.*s failed: %s", filter_len, topic_filter, MQTT_Status_strerror(st));
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "subscribed: %.*s", filter_len, topic_filter);
    return ESP_OK;
}

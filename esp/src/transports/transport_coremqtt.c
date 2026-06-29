/* transport_coremqtt.c — coreMQTT over mutual TLS to AWS IoT Core.
 *
 * coreMQTT has no outbox: MQTT_Publish serializes straight to the socket in the
 * caller's task and tracks nothing. To match esp-mqtt's semantics (and to make
 * QoS1 actually reliable) this file adds a small BOUNDED software outbox:
 *   - transport_publish() copies the message, enqueues it, and returns (non-blocking);
 *     when the byte/msg budget is exceeded it returns ESP_ERR_NO_MEM.
 *   - the mqtt task drains the queue (MQTT_Publish), keeps QoS1 messages in an
 *     in-flight set until their PUBACK arrives, retransmits on a timeout (DUP set),
 *     and re-sends every in-flight message after a reconnect.
 */
#include "transport.h"
#include "app_config.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "core_mqtt.h"
#include "network_transport.h"      /* esp-tls transport from coreMQTT port */
#include "backoff_algorithm.h"

static const char *TAG = "mqtt";

/* The TLS identity (endpoint, certs/key) comes from the resolved transport_config_t;
 * device_iot supplies the build-embedded certs via device_iot_default_config(). */

static MQTTContext_t s_mqtt_ctx;
static NetworkContext_t s_net_ctx;
static TransportInterface_t s_transport;
static uint8_t s_mqtt_buffer[MQTT_NETWORK_BUFFER_SIZE];
/* coreMQTT QoS>0 state records. WITHOUT MQTT_InitStatefulQoS these stay NULL and
 * MQTT_Publish rejects every QoS1 with MQTTBadParameter (and inbound QoS1 fails the
 * process loop). Outgoing sized to the outbox; a few incoming for QoS1 deliveries. */
static MQTTPubAckInfo_t s_outgoing_recs[MQTT_OUTBOX_MAX_MSGS];
static MQTTPubAckInfo_t s_incoming_recs[8];
static SemaphoreHandle_t s_mqtt_mutex;          /* serialises s_mqtt_ctx access */
static volatile bool s_connected;
static transport_publish_cb_t s_publish_cb;
static transport_conn_cb_t s_conn_cb;
static transport_config_t s_cfg;        /* resolved endpoint/thing/certs (held by ref) */

#define MQTT_LOCK()    xSemaphoreTakeRecursive(s_mqtt_mutex, portMAX_DELAY)
#define MQTT_UNLOCK()  xSemaphoreGiveRecursive(s_mqtt_mutex)

#define CONNACK_TIMEOUT_MS         5000U
#define RECONNECT_BACKOFF_BASE_MS  1000U
#define RECONNECT_BACKOFF_MAX_MS   16000U
#define RECONNECT_FOREVER          0U      /* 0 = retry forever (background task) */
#define QOS1_MAX_TRIES             8       /* give up (drop + free budget) after this */

/* ---- bounded software outbox -------------------------------------------- */
typedef struct {
    uint16_t topic_len;
    size_t   payload_len;
    uint8_t  qos;
    uint8_t  tries;        /* send attempts (drop after QOS1_MAX_TRIES) */
    uint16_t packetId;     /* assigned at first send (QoS>0) */
    uint32_t lastSentMs;
    bool     sent;
    char    *topic;        /* points into the same allocation, after the struct */
    uint8_t *payload;
} outbox_msg_t;

static QueueHandle_t   s_send_q;                          /* outbox_msg_t* to send */
static outbox_msg_t   *s_inflight[MQTT_OUTBOX_MAX_MSGS];  /* QoS1 awaiting PUBACK (task-only) */
static int             s_inflight_count;
static SemaphoreHandle_t s_budget_mux;                    /* guards the two counters */
static size_t          s_outbox_bytes;
static int             s_outbox_count;                    /* queued + in-flight */

static uint32_t getTimeMs(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

/* Reserve / release the shared byte+count budget (queued + in-flight). */
static bool budget_reserve(size_t sz)
{
    bool ok = false;
    xSemaphoreTake(s_budget_mux, portMAX_DELAY);
    if (s_outbox_count < MQTT_OUTBOX_MAX_MSGS &&
        s_outbox_bytes + sz <= MQTT_OUTBOX_LIMIT_BYTES) {
        s_outbox_bytes += sz;
        s_outbox_count++;
        ok = true;
    }
    xSemaphoreGive(s_budget_mux);
    return ok;
}
static void budget_release(size_t sz)
{
    xSemaphoreTake(s_budget_mux, portMAX_DELAY);
    s_outbox_bytes -= sz;
    s_outbox_count--;
    xSemaphoreGive(s_budget_mux);
}
static size_t msg_size(const outbox_msg_t *m) { return m->topic_len + m->payload_len; }

/* Serialize one outbox message to the socket (caller picks dup for retransmits). */
static void publish_msg(outbox_msg_t *m, bool dup)
{
    if (!s_connected) return;
    m->tries++;
    MQTT_LOCK();
    if (m->qos != 0 && m->packetId == 0) {
        m->packetId = MQTT_GetPacketId(&s_mqtt_ctx);
    }
    MQTTPublishInfo_t info = { 0 };
    info.qos = (MQTTQoS_t)m->qos;
    info.pTopicName = m->topic;
    info.topicNameLength = m->topic_len;
    info.pPayload = m->payload;
    info.payloadLength = m->payload_len;
    info.dup = dup;
    MQTTStatus_t st = MQTT_Publish(&s_mqtt_ctx, &info, m->packetId);
    MQTT_UNLOCK();

    if (st == MQTTSuccess) {
        m->sent = true;
        m->lastSentMs = getTimeMs();
    } else {
        ESP_LOGW(TAG, "publish to %.*s failed: %s", m->topic_len, m->topic,
                 MQTT_Status_strerror(st));
    }
}

/* Send everything newly queued, then retransmit any timed-out in-flight QoS1.
 * Runs only in the mqtt task while connected. */
static void drain_outbox(void)
{
    outbox_msg_t *m;
    while (xQueueReceive(s_send_q, &m, 0) == pdTRUE) {
        publish_msg(m, false);
        if (m->qos == 0) {
            size_t sz = msg_size(m);
            free(m);
            budget_release(sz);                  /* QoS0: send once, best effort */
        } else if (s_inflight_count < MQTT_OUTBOX_MAX_MSGS) {
            s_inflight[s_inflight_count++] = m;   /* QoS1: hold until PUBACK */
        } else {
            size_t sz = msg_size(m);
            free(m);
            budget_release(sz);                  /* unreachable: count budget caps this */
        }
    }
    uint32_t now = getTimeMs();
    for (int i = 0; i < s_inflight_count; i++) {
        outbox_msg_t *im = s_inflight[i];
        if (im->sent && (now - im->lastSentMs) <= MQTT_QOS1_RETRANSMIT_MS) {
            continue;                             /* sent recently, still awaiting PUBACK */
        }
        if (im->tries >= QOS1_MAX_TRIES) {        /* undeliverable -> give up, free budget */
            ESP_LOGW(TAG, "QoS1 to %.*s undelivered after %u tries -> dropping",
                     im->topic_len, im->topic, im->tries);
            size_t sz = msg_size(im);
            s_inflight[i] = s_inflight[--s_inflight_count];
            free(im);
            budget_release(sz);
            i--;                                  /* re-examine the moved entry */
            continue;
        }
        publish_msg(im, im->sent);                /* dup=true once it has been sent */
    }
}

/* PUBACK arrived (QoS1 delivered): drop the matching in-flight message. */
static void outbox_ack(uint16_t packetId)
{
    for (int i = 0; i < s_inflight_count; i++) {
        if (s_inflight[i]->packetId == packetId) {
            outbox_msg_t *m = s_inflight[i];
            size_t sz = msg_size(m);
            s_inflight[i] = s_inflight[--s_inflight_count];   /* compact */
            free(m);
            budget_release(sz);
            return;
        }
    }
}

static void mqttEventCallback(MQTTContext_t *pCtx,
                              MQTTPacketInfo_t *pPacketInfo,
                              MQTTDeserializedInfo_t *pDeserializedInfo)
{
    (void)pCtx;
    uint8_t type = pPacketInfo->type & 0xF0U;
    if (type == MQTT_PACKET_TYPE_PUBLISH) {
        MQTTPublishInfo_t *pub = pDeserializedInfo->pPublishInfo;
        if (s_publish_cb != NULL && pub != NULL) {
            s_publish_cb(pub->pTopicName, pub->topicNameLength,
                         (const uint8_t *)pub->pPayload, pub->payloadLength);
        }
    } else if (type == MQTT_PACKET_TYPE_PUBACK) {     /* QoS1 ack (runs in mqtt task) */
        outbox_ack(pDeserializedInfo->packetIdentifier);
    }
    /* SUBACK / PINGRESP: nothing to do. */
}

static void fill_network_context(void)
{
    memset(&s_net_ctx, 0, sizeof(s_net_ctx));
    s_net_ctx.pcHostname = s_cfg.endpoint;
    s_net_ctx.xPort      = s_cfg.port;
    s_net_ctx.disableSni = pdFALSE;

    /* Certs come straight from the config (NUL-terminated PEM, so the size includes
     * the NUL, which esp-tls expects). */
    s_net_ctx.pcServerRootCA     = s_cfg.root_ca_pem;
    s_net_ctx.pcServerRootCASize = strlen(s_cfg.root_ca_pem) + 1;
    s_net_ctx.pcClientCert       = s_cfg.client_cert_pem;
    s_net_ctx.pcClientCertSize   = strlen(s_cfg.client_cert_pem) + 1;

    /* Private key: the DS peripheral (use_secure_element makes the swap config-only)
     * or the PEM key. */
    if (s_cfg.use_secure_element) {
        s_net_ctx.use_secure_element = true;
        s_net_ctx.ds_data = s_cfg.ds_data;
    } else {
        s_net_ctx.pcClientKey = s_cfg.client_key_pem;
        s_net_ctx.pcClientKeySize = strlen(s_cfg.client_key_pem) + 1;
    }

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
        ESP_LOGE(TAG, "TLS connect to %s:%d failed", s_cfg.endpoint, s_cfg.port);
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

    /* REQUIRED for QoS>0: MQTT_Init leaves the publish-state records NULL, which
     * makes MQTT_Publish reject every QoS1 and inbound QoS1 fail the process loop.
     * Re-armed on each (re)connect; cleared first since this is a new session. */
    memset(s_outgoing_recs, 0, sizeof(s_outgoing_recs));
    memset(s_incoming_recs, 0, sizeof(s_incoming_recs));
    if (MQTT_InitStatefulQoS(&s_mqtt_ctx, s_outgoing_recs, MQTT_OUTBOX_MAX_MSGS,
                             s_incoming_recs, 8) != MQTTSuccess) {
        ESP_LOGE(TAG, "MQTT_InitStatefulQoS failed");
        xTlsDisconnect(&s_net_ctx);
        return ESP_FAIL;
    }

    MQTTConnectInfo_t connectInfo = { 0 };
    connectInfo.cleanSession = true;
    connectInfo.pClientIdentifier = s_cfg.thing_name;
    connectInfo.clientIdentifierLength = (uint16_t)strlen(s_cfg.thing_name);
    connectInfo.keepAliveSeconds = MQTT_KEEP_ALIVE_SECONDS;

    bool sessionPresent = false;
    MQTTStatus_t st = MQTT_Connect(&s_mqtt_ctx, &connectInfo, NULL,
                                   CONNACK_TIMEOUT_MS, &sessionPresent);
    if (st != MQTTSuccess) {
        ESP_LOGE(TAG, "MQTT_Connect failed: %s", MQTT_Status_strerror(st));
        xTlsDisconnect(&s_net_ctx);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MQTT connected to AWS IoT Core as '%s'", s_cfg.thing_name);
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
                /* New (cleanSession) session: force every in-flight QoS1 to resend. */
                for (int i = 0; i < s_inflight_count; i++) {
                    s_inflight[i]->lastSentMs = 0;
                }
                if (s_conn_cb != NULL) {
                    s_conn_cb(true);   /* device_iot re-subscribes app + OTA + birth */
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
            if (s_conn_cb != NULL) s_conn_cb(false);
            continue;
        }

        drain_outbox();                 /* send queued + retransmit unacked QoS1 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

esp_err_t transport_start(transport_publish_cb_t cb, const transport_config_t *cfg)
{
    s_publish_cb = cb;
    s_cfg = *cfg;        /* shallow copy: endpoint/thing_name/PEM held by reference */
    s_mqtt_mutex = xSemaphoreCreateRecursiveMutex();
    s_budget_mux = xSemaphoreCreateMutex();
    s_send_q = xQueueCreate(MQTT_OUTBOX_MAX_MSGS, sizeof(outbox_msg_t *));
    if (s_mqtt_mutex == NULL || s_budget_mux == NULL || s_send_q == NULL) {
        return ESP_ERR_NO_MEM;
    }

    /* NON-BLOCKING: the connect (and forever-reconnect) runs in mqtt_task, which
     * fires the conn cb on its first successful connect. device_iot decides whether
     * to wait for connectivity (trial gate) — the transport never blocks the boot.
     * Generous stack: the connect path runs the mbedTLS handshake there. */
    s_connected = false;
    if (xTaskCreate(mqtt_task, "mqtt", 8192, NULL, 5, NULL) != pdPASS) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

void transport_set_conn_cb(transport_conn_cb_t cb)
{
    s_conn_cb = cb;
}

bool transport_is_connected(void)
{
    return s_connected;
}

esp_err_t transport_publish(const char *topic, uint16_t topic_len,
                              const void *payload, size_t payload_len, uint8_t qos)
{
    if (!s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    size_t sz = (size_t)topic_len + payload_len;
    if (!budget_reserve(sz)) {
        return ESP_ERR_NO_MEM;                 /* uniform backpressure signal */
    }

    /* One allocation holds the struct, the NUL-terminated topic, and the payload. */
    outbox_msg_t *m = malloc(sizeof(*m) + topic_len + 1 + payload_len);
    if (m == NULL) {
        budget_release(sz);
        return ESP_ERR_NO_MEM;
    }
    m->topic = (char *)(m + 1);
    m->payload = (uint8_t *)m->topic + topic_len + 1;
    memcpy(m->topic, topic, topic_len);
    m->topic[topic_len] = '\0';
    if (payload_len) memcpy(m->payload, payload, payload_len);
    m->topic_len = topic_len;
    m->payload_len = payload_len;
    m->qos = qos;
    m->tries = 0;
    m->packetId = 0;
    m->lastSentMs = 0;
    m->sent = false;

    if (xQueueSend(s_send_q, &m, 0) != pdTRUE) {
        free(m);
        budget_release(sz);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t transport_subscribe(const char *topic_filter, uint16_t filter_len, uint8_t qos)
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

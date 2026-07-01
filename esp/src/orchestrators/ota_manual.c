/*
 * control_protocol.c — variant A: hand-rolled MQTT OTA control plane.
 *
 * Same self-test / commit / rollback / NVS hand-off as variant C, but the
 * orchestration is a custom request/response on app topics instead of AWS IoT
 * Jobs. No Jobs lib, no coreJSON — just esp-mqtt + cJSON + esp_https_ota.
 */
#include "ota_backend.h"
#include "device_iot.h"
#include "app_config.h"
#include "transport.h"
#include "self_test.h"
#include "ota_download.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "cJSON.h"

static const char *TAG = "ctrl";

#define EVT_ASK       1     /* publish should-i-update */
#define EVT_PLAN      2     /* a plan arrived */
#define PLAN_BUF_SIZE 1024U

static char    s_planBuf[PLAN_BUF_SIZE];
static size_t  s_planLen;
static QueueHandle_t s_evt_queue;
static volatile bool s_ready;

/* ---- topic helpers: dt/<thing>/ota/<suffix> ---- */
static int topic_for(char *buf, size_t buflen, const char *suffix)
{
    return snprintf(buf, buflen, "dt/%s/ota/%s", device_iot_thing_name(), suffix);
}

static void publishShouldIUpdate(void)
{
    char topic[160];
    int tlen = topic_for(topic, sizeof(topic), "should-i-update");
    char payload[96];
    int n = snprintf(payload, sizeof(payload), "{\"thing\":\"%s\",\"version\":\"%d.%d.%d\"}",
                     device_iot_thing_name(), APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);
    ESP_LOGI(TAG, "asking server: should-i-update? (v%d.%d.%d)",
             APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);
    transport_publish(topic, (uint16_t)tlen, payload, (size_t)n, 1);
}

static void publishConfirm(const char *ota_id, const char *result)
{
    char topic[160];
    int tlen = topic_for(topic, sizeof(topic), "confirm");
    char payload[160];
    int n = snprintf(payload, sizeof(payload),
                     "{\"ota_id\":\"%s\",\"result\":\"%s\",\"version\":\"%d.%d.%d\"}",
                     ota_id, result, APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);
    ESP_LOGI(TAG, "confirming ota_id=%s -> %s", ota_id, result);
    transport_publish(topic, (uint16_t)tlen, payload, (size_t)n, 1);
}

/* Parse {"op":"ota","url","ota_id","target_version"} and act. */
static void processPlan(void)
{
    cJSON *root = cJSON_ParseWithLength(s_planBuf, s_planLen);
    if (root == NULL) {
        ESP_LOGE(TAG, "plan is not valid JSON");
        return;
    }
    const cJSON *op  = cJSON_GetObjectItem(root, "op");
    const cJSON *url = cJSON_GetObjectItem(root, "url");
    const cJSON *oid = cJSON_GetObjectItem(root, "ota_id");
    const cJSON *tgt = cJSON_GetObjectItem(root, "target_version");

    if (cJSON_IsString(op) && strcmp(op->valuestring, "ota") == 0 &&
        cJSON_IsString(url) && cJSON_IsString(oid)) {
        ESP_LOGI(TAG, "update plan: ota_id=%s target=%s", oid->valuestring,
                 cJSON_IsString(tgt) ? tgt->valuestring : "?");
        ota_nvs_set_job_id(oid->valuestring);   /* "job_id" key holds the ota_id */

        if (ota_download_run(url->valuestring) == ESP_OK) {
            ESP_LOGI(TAG, "rebooting into new image...");
            cJSON_Delete(root);
            esp_restart();                       /* does not return */
        }
        ESP_LOGE(TAG, "download failed -> confirming failed");
        publishConfirm(oid->valuestring, "failed");
        ota_nvs_clear_job_id();
    } else {
        ESP_LOGI(TAG, "server says: no update");
    }
    cJSON_Delete(root);
}

/* --------------------------- event routing ------------------------------- */
void ota_backend_on_publish(const char *topic, size_t topic_len,
                                 const uint8_t *payload, size_t payload_len)
{
    if (!s_ready) {
        return;
    }
    char planTopic[160];
    topic_for(planTopic, sizeof(planTopic), "plan");
    if (strncmp(topic, planTopic, topic_len) == 0 && strlen(planTopic) == topic_len) {
        size_t n = payload_len < sizeof(s_planBuf) ? payload_len : sizeof(s_planBuf) - 1;
        memcpy(s_planBuf, payload, n);
        s_planBuf[n] = '\0';
        s_planLen = n;
        int evt = EVT_PLAN;
        xQueueSend(s_evt_queue, &evt, 0);
    }
}

/* --------------------------- task + startup ------------------------------ */
static void otaTask(void *arg)
{
    (void)arg;
    int evt;
    for (;;) {
        if (xQueueReceive(s_evt_queue, &evt, portMAX_DELAY) == pdTRUE) {
            if (evt == EVT_ASK) {
                publishShouldIUpdate();
            } else if (evt == EVT_PLAN) {
                processPlan();
            }
        }
    }
}

static void subscribeControlTopics(void)
{
    char topic[160];
    int tlen = topic_for(topic, sizeof(topic), "plan");
    transport_subscribe(topic, (uint16_t)tlen, 1);
    tlen = topic_for(topic, sizeof(topic), "ack");
    transport_subscribe(topic, (uint16_t)tlen, 1);
}

void ota_backend_on_reconnect(void)
{
    subscribeControlTopics();
    int evt = EVT_ASK;
    xQueueSend(s_evt_queue, &evt, 0);
}

/* Trial-boot self-test gate — identical contract to variant C, but the report
 * is a custom "confirm" publish instead of a Jobs update. */
static void resolve_trial_boot(void)
{
    char ota_id[64] = { 0 };
    bool haveId = (ota_nvs_get_job_id(ota_id, sizeof(ota_id)) == ESP_OK);
    ESP_LOGW(TAG, "trial boot detected — running self-test%s", haveId ? "" : " (no stashed ota id)");

    bool cloud_ok = transport_is_connected();
    bool core_ok  = self_test_core_function_ok();

    if (cloud_ok && core_ok) {
        self_test_commit();
        self_test_disarm_watchdog();
        if (haveId) {
            publishConfirm(ota_id, "success");
            ota_nvs_clear_job_id();
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    } else {
        ESP_LOGE(TAG, "self-test failed (cloud=%d core=%d) -> rollback", cloud_ok, core_ok);
        if (haveId) {
            publishConfirm(ota_id, "failed");
            vTaskDelay(pdMS_TO_TICKS(1500));
            ota_nvs_clear_job_id();
        }
        self_test_reject_and_rollback();   /* does not return */
    }
}

void ota_backend_start(void)
{
    s_evt_queue = xQueueCreate(8, sizeof(int));
    configASSERT(s_evt_queue != NULL);

    if (self_test_is_trial_boot()) {
        resolve_trial_boot();
    }

    s_ready = true;

    if (xTaskCreate(otaTask, "ctrl_ota", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create control task");
        return;
    }
    /* subscribe + ask happen via the connect path (ota_backend_on_reconnect),
     * driven by device_iot — exactly once per connect. */
}

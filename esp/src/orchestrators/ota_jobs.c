/*
 * jobs_orchestrator.c — variant B: AWS IoT Jobs (esp-mqtt) + esp_https_ota.
 *
 * Same job-control shape as variant C's ota_orchestrator.c (StartNext,
 * notify-next + start-next/accepted, IN_PROGRESS/SUCCEEDED/FAILED reporting,
 * trial-boot self-test gate), but the AFR File-Streams download + PAL +
 * code-signature verify are GONE: the job document carries a presigned S3 URL
 * which esp_https_ota fetches directly.
 */
#include "ota_backend.h"
#include "device_iot.h"
#include "app_config.h"
#include "transport.h"
#include "self_test.h"
#include "ota_download.h"
#include "ota_jobs_common.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"

#include "jobs.h"     /* standalone AWS IoT Jobs lib (topic helpers only) */
#include "cJSON.h"    /* IDF-native json component */

static const char *TAG = "jobs";

#define MAX_JOB_ID_LENGTH   64U
#define JOB_DOC_SIZE        2048U
#define EVT_REQUEST_JOB     1
#define EVT_JOB_DOC         2

static char     globalJobId[MAX_JOB_ID_LENGTH];
static char     s_jobDoc[JOB_DOC_SIZE];
static size_t   s_jobDocLen;
static QueueHandle_t s_evt_queue;
static volatile bool s_ready;

/* The Jobs control plane (StartNext request, Jobs_Update status report,
 * start-next/accepted subscribe, and the post-commit idempotency guard) is shared
 * across the mqtt/https/jobs backends — see ota_jobs_common.c. */

/* ------------------------- job-doc processing ---------------------------- */
/* Parse the CUSTOM document {"op":"ota","url":...,"target_version":...},
 * download via HTTPS, then reboot into the trial image. */
static void processJobDoc(void)
{
    const char *jobId;
    size_t jobIdLen = Jobs_GetJobId(s_jobDoc, s_jobDocLen, &jobId);
    if (jobIdLen == 0 || jobIdLen >= MAX_JOB_ID_LENGTH) {
        return;
    }
    if (ota_jobs_is_completed(jobId, jobIdLen)) {
        return;
    }
    memcpy(globalJobId, jobId, jobIdLen);
    globalJobId[jobIdLen] = '\0';

    const char *doc;
    size_t docLen = Jobs_GetJobDocument(s_jobDoc, s_jobDocLen, &doc);
    if (docLen == 0) {
        ESP_LOGI(TAG, "no job document");
        return;
    }

    cJSON *root = cJSON_ParseWithLength(doc, docLen);
    if (root == NULL) {
        ESP_LOGE(TAG, "job document is not valid JSON");
        return;
    }
    const cJSON *op  = cJSON_GetObjectItem(root, "op");
    const cJSON *url = cJSON_GetObjectItem(root, "url");
    const cJSON *tgt = cJSON_GetObjectItem(root, "target_version");

    if (cJSON_IsString(op) && strcmp(op->valuestring, "ota") == 0 && cJSON_IsString(url)) {
        ESP_LOGI(TAG, "OTA job: target=%s", cJSON_IsString(tgt) ? tgt->valuestring : "?");
        /* Promote QUEUED -> IN_PROGRESS (notify-next pushes the doc while QUEUED). */
        ota_jobs_report_status(globalJobId, "IN_PROGRESS");
        ota_nvs_set_job_id(globalJobId);   /* hand-off across the reboot */

        if (ota_download_run(url->valuestring) == ESP_OK) {
            ESP_LOGI(TAG, "rebooting into new image...");
            cJSON_Delete(root);
            esp_restart();                 /* does not return */
        }
        ESP_LOGE(TAG, "download failed -> reporting FAILED");
        ota_jobs_report_status(globalJobId, "FAILED");
        ota_nvs_clear_job_id();
        globalJobId[0] = '\0';
    } else {
        ESP_LOGI(TAG, "not an OTA job (op != \"ota\")");
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
    /* Deferred-to-boot: only the StartNext response, not the notify-next push. */
    bool isJob = Jobs_IsStartNextAccepted(topic, topic_len, device_iot_thing_name(), strlen(device_iot_thing_name()));
    if (isJob) {
        size_t n = payload_len < sizeof(s_jobDoc) ? payload_len : sizeof(s_jobDoc) - 1;
        memcpy(s_jobDoc, payload, n);
        s_jobDoc[n] = '\0';
        s_jobDocLen = n;
        int evt = EVT_JOB_DOC;
        xQueueSend(s_evt_queue, &evt, 0);
    }
}

/* --------------------------- task + startup ------------------------------ */
static void otaTask(void *arg)
{
    (void)arg;
    /* No initial StartNext here: the connect path (ota_backend_on_reconnect) drives
     * subscribe+request, so it fires exactly once. */
    int evt;
    for (;;) {
        if (xQueueReceive(s_evt_queue, &evt, portMAX_DELAY) == pdTRUE) {
            if (evt == EVT_REQUEST_JOB) {
                ESP_LOGI(TAG, "requesting job document (StartNext)");
                ota_jobs_request_document();
            } else if (evt == EVT_JOB_DOC) {
                processJobDoc();
            }
        }
    }
}

void ota_backend_on_reconnect(void)
{
    ota_jobs_subscribe_topics();
    int evt = EVT_REQUEST_JOB;
    xQueueSend(s_evt_queue, &evt, 0);
}

void ota_backend_start(void)
{
    s_evt_queue = xQueueCreate(8, sizeof(int));
    configASSERT(s_evt_queue != NULL);

    if (self_test_is_trial_boot()) {
        self_test_resolve_trial(ota_jobs_trial_report, 5000);
    }

    s_ready = true;

    if (xTaskCreate(otaTask, "jobs_ota", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create OTA task");
    }
}

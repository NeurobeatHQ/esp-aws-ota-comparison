/*
 * jobs_orchestrator.c — variant B: AWS IoT Jobs (esp-mqtt) + esp_https_ota.
 *
 * Same job-control shape as variant C's ota_orchestrator.c (StartNext,
 * notify-next + start-next/accepted, IN_PROGRESS/SUCCEEDED/FAILED reporting,
 * trial-boot self-test gate), but the AFR File-Streams download + PAL +
 * code-signature verify are GONE: the job document carries a presigned S3 URL
 * which esp_https_ota fetches directly.
 */
#include "jobs_orchestrator.h"
#include "app_config.h"
#include "mqtt_es.h"
#include "self_test.h"
#include "ota_download.h"

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

#define NOTIFY_NEXT_SUFFIX  "/jobs/notify-next"

static char     globalJobId[MAX_JOB_ID_LENGTH];
static char     s_completedJobId[MAX_JOB_ID_LENGTH];
static char     s_jobDoc[JOB_DOC_SIZE];
static size_t   s_jobDocLen;
static QueueHandle_t s_evt_queue;
static volatile bool s_ready;

/* ------------------------- Jobs control plane ---------------------------- */
static void requestJobDocumentHandler(void)
{
    char topic[TOPIC_BUFFER_SIZE + 1] = { 0 };
    char msg[START_JOB_MSG_LENGTH] = { 0 };
    size_t topicLen = 0;
    if (Jobs_StartNext(topic, TOPIC_BUFFER_SIZE, THING_NAME, strlen(THING_NAME),
                       &topicLen) == JobsSuccess) {
        size_t msgLen = Jobs_StartNextMsg("token", 5U, msg, START_JOB_MSG_LENGTH);
        if (msgLen > 0) {
            mqtt_es_publish(topic, (uint16_t)topicLen, msg, msgLen, 0);
        }
    }
}

static bool reportJobStatus(const char *jobId, const char *status)
{
    char topic[TOPIC_BUFFER_SIZE + 1] = { 0 };
    char msg[UPDATE_JOB_MSG_LENGTH + 16] = { 0 };
    size_t topicLen = 0;
    if (Jobs_Update(topic, TOPIC_BUFFER_SIZE, THING_NAME, strlen(THING_NAME),
                    jobId, (uint16_t)strnlen(jobId, MAX_JOB_ID_LENGTH), &topicLen) != JobsSuccess) {
        return false;
    }
    int n = snprintf(msg, sizeof(msg), "%s%s\"}", JOBS_API_STATUS, status);
    if (n <= 0) {
        return false;
    }
    ESP_LOGI(TAG, "reporting job %s -> %s", jobId, status);
    return mqtt_es_publish(topic, (uint16_t)topicLen, msg, (size_t)n, 0) == ESP_OK;
}

static void prvSubscribeJobTopics(void)
{
    char topic[JOBS_API_MAX_LENGTH(sizeof(THING_NAME))] = { 0 };
    size_t len = 0;
    if (Jobs_GetTopic(topic, sizeof(topic), THING_NAME, strlen(THING_NAME),
                      JobsNextJobChanged, &len) == JobsSuccess) {
        mqtt_es_subscribe(topic, (uint16_t)len, 1);
    }
    len = 0;
    if (Jobs_GetTopic(topic, sizeof(topic), THING_NAME, strlen(THING_NAME),
                      JobsStartNextSuccess, &len) == JobsSuccess) {
        mqtt_es_subscribe(topic, (uint16_t)len, 1);
    }
}

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
    if (s_completedJobId[0] != '\0' && strncmp(s_completedJobId, jobId, jobIdLen) == 0) {
        ESP_LOGI(TAG, "ignoring already-completed job (re-affirming SUCCEEDED)");
        reportJobStatus(s_completedJobId, "SUCCEEDED");
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
        reportJobStatus(globalJobId, "IN_PROGRESS");
        ota_nvs_set_job_id(globalJobId);   /* hand-off across the reboot */

        if (ota_download_run(url->valuestring) == ESP_OK) {
            ESP_LOGI(TAG, "rebooting into new image...");
            cJSON_Delete(root);
            esp_restart();                 /* does not return */
        }
        ESP_LOGE(TAG, "download failed -> reporting FAILED");
        reportJobStatus(globalJobId, "FAILED");
        ota_nvs_clear_job_id();
        globalJobId[0] = '\0';
    } else {
        ESP_LOGI(TAG, "not an OTA job (op != \"ota\")");
    }
    cJSON_Delete(root);
}

/* --------------------------- event routing ------------------------------- */
void jobs_orchestrator_on_publish(const char *topic, size_t topic_len,
                                  const uint8_t *payload, size_t payload_len)
{
    if (!s_ready) {
        return;
    }
    bool isJob = Jobs_IsStartNextAccepted(topic, topic_len, THING_NAME, strlen(THING_NAME));
    if (!isJob) {
        /* notify-next push (topic is NUL-terminated by mqtt_es). */
        isJob = (strstr(topic, NOTIFY_NEXT_SUFFIX) != NULL);
    }
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
    /* Let the SUBSCRIBEs settle before the first StartNext. */
    vTaskDelay(pdMS_TO_TICKS(1000));
    int evt = EVT_REQUEST_JOB;
    xQueueSend(s_evt_queue, &evt, 0);

    for (;;) {
        if (xQueueReceive(s_evt_queue, &evt, portMAX_DELAY) == pdTRUE) {
            if (evt == EVT_REQUEST_JOB) {
                ESP_LOGI(TAG, "requesting job document (StartNext)");
                requestJobDocumentHandler();
            } else if (evt == EVT_JOB_DOC) {
                processJobDoc();
            }
        }
    }
}

static void prvOnReconnect(void)
{
    prvSubscribeJobTopics();
    int evt = EVT_REQUEST_JOB;
    xQueueSend(s_evt_queue, &evt, 0);
}

/* Trial-boot self-test gate — identical contract to variant C. */
static void resolve_trial_boot(void)
{
    char jobId[MAX_JOB_ID_LENGTH] = { 0 };
    bool haveJob = (ota_nvs_get_job_id(jobId, sizeof(jobId)) == ESP_OK);
    ESP_LOGW(TAG, "trial boot detected — running self-test%s", haveJob ? "" : " (no stashed job id)");

    bool cloud_ok = mqtt_es_is_connected();
    bool core_ok  = self_test_core_function_ok();

    if (cloud_ok && core_ok) {
        self_test_commit();
        self_test_disarm_watchdog();
        if (haveJob) {
            reportJobStatus(jobId, "SUCCEEDED");
            strncpy(s_completedJobId, jobId, sizeof(s_completedJobId) - 1);
            ota_nvs_clear_job_id();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));   /* let IoT mark the job SUCCEEDED */
    } else {
        ESP_LOGE(TAG, "self-test failed (cloud=%d core=%d) -> rollback", cloud_ok, core_ok);
        if (haveJob) {
            reportJobStatus(jobId, "FAILED");
            ota_nvs_clear_job_id();
            vTaskDelay(pdMS_TO_TICKS(1500));
        }
        self_test_reject_and_rollback();   /* does not return */
    }
}

void jobs_orchestrator_start(void)
{
    s_evt_queue = xQueueCreate(8, sizeof(int));
    configASSERT(s_evt_queue != NULL);

    if (self_test_is_trial_boot()) {
        resolve_trial_boot();
    }

    prvSubscribeJobTopics();
    mqtt_es_set_reconnect_cb(prvOnReconnect);
    s_ready = true;

    if (xTaskCreate(otaTask, "jobs_ota", 6144, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create OTA task");
    }
}

/*
 * ota_jobs_common.c — shared AWS IoT Jobs control plane (see ota_jobs_common.h).
 *
 * Hoisted verbatim from ota_filestreams.c / ota_http.c / ota_jobs.c, where these
 * functions were duplicated. The per-backend data path (how the firmware bytes are
 * fetched) stays in each orchestrator; only this Jobs request/report/subscribe +
 * idempotency scaffolding is shared.
 */
#include "ota_jobs_common.h"
#include "device_iot.h"
#include "app_config.h"
#include "transport.h"
#include "self_test.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "jobs.h"

static const char *TAG = "ota";

#define MAX_JOB_ID_LENGTH   64U

/* The job id committed during the last trial boot; guards re-processing it. */
static char s_completedJobId[MAX_JOB_ID_LENGTH];

void ota_jobs_request_document(void)
{
    char topicBuffer[TOPIC_BUFFER_SIZE + 1] = { 0 };
    char messageBuffer[START_JOB_MSG_LENGTH] = { 0 };
    size_t topicLength = 0;
    if (Jobs_StartNext(topicBuffer, TOPIC_BUFFER_SIZE,
                       device_iot_thing_name(), strlen(device_iot_thing_name()), &topicLength) == JobsSuccess) {
        size_t msgLen = Jobs_StartNextMsg("token", 5U, messageBuffer, START_JOB_MSG_LENGTH);
        if (msgLen > 0) {
            transport_publish(topicBuffer, topicLength, messageBuffer, msgLen, 0);
        }
    }
}

bool ota_jobs_report_status(const char *jobId, const char *status)
{
    char topicBuffer[TOPIC_BUFFER_SIZE + 1] = { 0 };
    char messageBuffer[UPDATE_JOB_MSG_LENGTH + 16] = { 0 };
    size_t topicLength = 0;
    if (Jobs_Update(topicBuffer, TOPIC_BUFFER_SIZE, device_iot_thing_name(), strlen(device_iot_thing_name()),
                    jobId, (uint16_t)strnlen(jobId, MAX_JOB_ID_LENGTH), &topicLength) != JobsSuccess) {
        return false;
    }
    int n = snprintf(messageBuffer, sizeof(messageBuffer), "%s%s\"}", JOBS_API_STATUS, status);
    if (n <= 0) {
        return false;
    }
    ESP_LOGI(TAG, "reporting job %s -> %s", jobId, status);
    return transport_publish(topicBuffer, topicLength, messageBuffer, (uint32_t)n, 0) == ESP_OK;
}

void ota_jobs_subscribe_topics(void)
{
    char topic[JOBS_API_MAX_LENGTH(MAX_THING_NAME_LEN)] = { 0 };
    size_t len = 0;
    /* Deferred-to-boot: NOT subscribed to notify-next (the live push). The device
     * learns about a job only via its own StartNext at (re)connect, so a job queued
     * while running waits until the next boot/connect. Only start-next/accepted. */
    if (Jobs_GetTopic(topic, sizeof(topic), device_iot_thing_name(), strlen(device_iot_thing_name()),
                      JobsStartNextSuccess, &len) == JobsSuccess) {
        transport_subscribe(topic, (uint16_t)len, 1);
    }
}

bool ota_jobs_is_completed(const char *jobId, size_t jobIdLen)
{
    if (s_completedJobId[0] != '\0' && strlen(s_completedJobId) == jobIdLen &&
        strncmp(s_completedJobId, jobId, jobIdLen) == 0) {          /* full equality, not prefix */
        ESP_LOGI(TAG, "ignoring already-completed job (re-affirming SUCCEEDED)");
        ota_jobs_report_status(s_completedJobId, "SUCCEEDED");
        return true;
    }
    return false;
}

void ota_jobs_trial_report(const char *jobId, bool ok)
{
    if (ok) {
        ota_jobs_report_status(jobId, "SUCCEEDED");
        strncpy(s_completedJobId, jobId, sizeof(s_completedJobId) - 1);
        ota_nvs_clear_job_id();
    } else {
        ota_jobs_report_status(jobId, "FAILED");
        ota_nvs_clear_job_id();
        vTaskDelay(pdMS_TO_TICKS(1500));   /* best-effort flush before reboot */
    }
}

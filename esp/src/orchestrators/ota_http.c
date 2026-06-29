/*
 * ota_orchestrator.c — variant D: AWS OTA agent stack with the HTTP data path.
 *
 * Identical to variant C (mqtt-esp) — coreMQTT control plane, AWS IoT Jobs,
 * the ESP32 OTA PAL with the AWS Signer ECDSA-P256 signature check, and the
 * self-test/commit/rollback gate — EXCEPT the bulk firmware transfer: instead of
 * AWS MQTT File Streams, the device streams the image over an HTTPS GET of the
 * presigned S3 URL the OTA service puts in the job document (afr_ota.protocols
 * = ["HTTP"], parsed into jobFields.imageRef), feeding each chunk to the same PAL.
 *
 * Flow:
 *   notify-next / StartNext -> AFR_OTA(HTTP) job doc -> esp_http_client GET ->
 *   otaPal_WriteBlock -> otaPal_CloseFile (ECDSA verify) -> activate + reboot ->
 *   [trial boot] self-test -> commit (SUCCEEDED) or reject (rollback/FAILED).
 */
#include "ota_backend.h"
#include "device_iot.h"
#include "app_config.h"
#include "transport.h"
#include "self_test.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"

#include "core_mqtt.h"          /* MQTT_MatchTopic */
#include "MQTTFileDownloader_base64.h"   /* base64_Decode for the signature */
#include "jobs.h"
#include "job_parser.h"
#include "ota_job_processor.h"
#include "ota_os_freertos.h"
#include "ota_pal.h"

static const char *TAG = "ota";

/* Certs embedded as generated C arrays (embed_certs.cmake): the code-signing
 * cert for the PAL's ECDSA verify, and the AWS root CA for the S3 HTTPS GET. */
extern const unsigned char codesign_cert_pem[];
extern const unsigned char aws_root_ca_pem[];

#define OTA_TASK_STACK_SIZE        8192
#define OTA_TASK_PRIORITY          4
#define MAX_JOB_ID_LENGTH          64U
#define OTA_MAX_SIGNATURE_SIZE     384U
#define HTTP_CHUNK_SIZE            4096U
#define MAX_URL_LEN                1024U

#define OTA_TOPIC_PREFIX                   "$aws/things/+/"
#define OTA_JOB_NOTIFY_TOPIC_FILTER        OTA_TOPIC_PREFIX "jobs/notify-next"
#define OTA_JOB_NOTIFY_TOPIC_FILTER_LEN    ((uint16_t)(sizeof(OTA_JOB_NOTIFY_TOPIC_FILTER) - 1))

typedef enum { OtaMqttSuccess = 0, OtaMqttPublishFailed, OtaMqttSubscribeFailed } OtaMqttStatus_t;

static char     globalJobId[MAX_JOB_ID_LENGTH];
static char     s_completedJobId[MAX_JOB_ID_LENGTH];
static OtaJobEventData_t    jobDocBuffer;
static AfrOtaJobDocumentFields_t jobFields;
static uint8_t  OtaImageSignatureDecoded[OTA_MAX_SIGNATURE_SIZE];
static OtaState_t otaAgentState = OtaAgentStateInit;
static volatile bool s_ready;

static void prvSubscribeJobTopics(void);

/* ------------------------- MQTT thin wrappers ---------------------------- */
static OtaMqttStatus_t prvPublish(const char *topic, uint16_t topicLen,
                                  const char *msg, uint32_t msgLen, uint8_t qos)
{
    return (transport_publish(topic, topicLen, msg, msgLen, qos) == ESP_OK)
               ? OtaMqttSuccess : OtaMqttPublishFailed;
}

static OtaMqttStatus_t prvSubscribe(const char *filter, uint16_t filterLen, uint8_t qos)
{
    return (transport_subscribe(filter, filterLen, qos) == ESP_OK)
               ? OtaMqttSuccess : OtaMqttSubscribeFailed;
}

/* ------------------------- Jobs control plane ---------------------------- */
static void requestJobDocumentHandler(void)
{
    char topicBuffer[TOPIC_BUFFER_SIZE + 1] = { 0 };
    char messageBuffer[START_JOB_MSG_LENGTH] = { 0 };
    size_t topicLength = 0;
    if (Jobs_StartNext(topicBuffer, TOPIC_BUFFER_SIZE,
                       device_iot_thing_name(), strlen(device_iot_thing_name()), &topicLength) == JobsSuccess) {
        size_t msgLen = Jobs_StartNextMsg("token", 5U, messageBuffer, START_JOB_MSG_LENGTH);
        if (msgLen > 0) {
            prvPublish(topicBuffer, topicLength, messageBuffer, msgLen, 0);
        }
    }
}

static bool reportJobStatus(const char *jobId, const char *status)
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
    return prvPublish(topicBuffer, topicLength, messageBuffer, (uint32_t)n, 0) == OtaMqttSuccess;
}

static bool jobDocumentParser(char *message, size_t messageLength,
                              AfrOtaJobDocumentFields_t *fields)
{
    const char *jobDoc;
    size_t jobDocLength = Jobs_GetJobDocument(message, messageLength, &jobDoc);
    int8_t fileIndex = 0;
    if (jobDocLength != 0U) {
        do {
            /* The parser reads afr_ota.protocols[0] from the doc: for protocols
             * ["HTTP"] it puts the presigned URL in fields->imageRef. */
            fileIndex = otaParser_parseJobDocFile(jobDoc, jobDocLength, fileIndex, fields);
        } while (fileIndex > 0);
    }
    return fileIndex == 0;
}

/* AWS delivers the signature base64-encoded; the PAL wants DER. */
static bool convertSignatureToDER(AfrOtaJobDocumentFields_t *fields)
{
    size_t decodedLength = 0;
    Base64Status_t r = base64_Decode(OtaImageSignatureDecoded, sizeof(OtaImageSignatureDecoded),
                                     &decodedLength, (const uint8_t *)fields->signature,
                                     fields->signatureLen);
    if (r != Base64Success) {
        return false;
    }
    fields->signature = (const char *)OtaImageSignatureDecoded;
    fields->signatureLen = decodedLength;
    return true;
}

/* --------------------- the HTTP data path (vs C's MQTT) ------------------ */
/* Stream the firmware from the presigned URL straight into the OTA PAL. The PAL
 * (otaPal_WriteBlock) writes to the passive partition exactly as it did for the
 * MQTT blocks, so the subsequent otaPal_CloseFile ECDSA verify is unchanged. */
static esp_err_t http_download_to_pal(AfrOtaJobDocumentFields_t *fields)
{
    if (fields->imageRefLen == 0 || fields->imageRefLen >= MAX_URL_LEN) {
        ESP_LOGE(TAG, "missing/oversize presigned URL (len=%u)", (unsigned)fields->imageRefLen);
        return ESP_ERR_INVALID_ARG;
    }
    char url[MAX_URL_LEN];
    memcpy(url, fields->imageRef, fields->imageRefLen);
    url[fields->imageRefLen] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .cert_pem = (const char *)aws_root_ca_pem,   /* S3 chains to Amazon Root CA 1 */
        .timeout_ms = 20000,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    int64_t content_len = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "HTTP status %d (presigned URL expired?)", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "downloading %lld bytes over HTTPS", (long long)content_len);

    static uint8_t buf[HTTP_CHUNK_SIZE];
    uint32_t offset = 0;
    int last_pct = 0;
    int r;
    while ((r = esp_http_client_read(client, (char *)buf, sizeof(buf))) > 0) {
        int16_t w = otaPal_WriteBlock(fields, offset, buf, (uint32_t)r);
        if (w < 0) {
            ESP_LOGE(TAG, "otaPal_WriteBlock failed at offset %lu", (unsigned long)offset);
            err = ESP_FAIL;
            break;
        }
        offset += (uint32_t)r;
        if (fields->fileSize && (int)(offset * 100 / fields->fileSize) - last_pct >= 10) {
            last_pct = offset * 100 / fields->fileSize;
            ESP_LOGI(TAG, "downloaded %lu / %lu bytes (%d%%)",
                     (unsigned long)offset, (unsigned long)fields->fileSize, last_pct);
        }
    }
    if (r < 0) {
        err = ESP_FAIL;
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err == ESP_OK && offset == fields->fileSize) {
        ESP_LOGI(TAG, "download complete (%lu bytes)", (unsigned long)offset);
        return ESP_OK;
    }
    ESP_LOGE(TAG, "download incomplete: %lu / %lu bytes", (unsigned long)offset,
             (unsigned long)fields->fileSize);
    return ESP_FAIL;
}

/* Returns true if a job was parsed + accepted for download (fields populated). */
static bool receivedJobDocumentHandler(OtaJobEventData_t *jobDoc)
{
    const char *jobId;
    size_t jobIdLength = Jobs_GetJobId((char *)jobDoc->jobData, jobDoc->jobDataLength, &jobId);
    if (jobIdLength == 0) {
        return false;
    }
    if (s_completedJobId[0] != '\0' && strlen(s_completedJobId) == jobIdLength &&
        strncmp(s_completedJobId, jobId, jobIdLength) == 0) {       /* full equality, not prefix */
        ESP_LOGI(TAG, "ignoring already-completed job (re-affirming SUCCEEDED)");
        reportJobStatus(s_completedJobId, "SUCCEEDED");
        return false;
    }
    if (jobIdLength >= MAX_JOB_ID_LENGTH) {
        return false;
    }
    memset(&jobFields, 0, sizeof(jobFields));
    memcpy(globalJobId, jobId, jobIdLength);
    globalJobId[jobIdLength] = '\0';

    if (!jobDocumentParser((char *)jobDoc->jobData, jobDoc->jobDataLength, &jobFields)) {
        ESP_LOGI(TAG, "not an OTA job document");
        return false;
    }
    if (!convertSignatureToDER(&jobFields)) {
        ESP_LOGE(TAG, "failed to decode image signature");
        return false;
    }
    if (otaPal_CreateFileForRx(&jobFields) != OtaPalSuccess) {
        return false;
    }
    ESP_LOGI(TAG, "OTA job accepted (HTTP data path), %lu bytes",
             (unsigned long)jobFields.fileSize);
    reportJobStatus(globalJobId, "IN_PROGRESS");
    return true;
}

/* --------------------------- event routing ------------------------------- */
void ota_backend_on_publish(const char *topic, size_t topic_len,
                                 const uint8_t *payload, size_t payload_len)
{
    if (!s_ready) {
        return;
    }
    bool isJob = Jobs_IsStartNextAccepted(topic, topic_len, device_iot_thing_name(), strlen(device_iot_thing_name()));
    if (!isJob) {
        (void)MQTT_MatchTopic(topic, topic_len, OTA_JOB_NOTIFY_TOPIC_FILTER,
                              OTA_JOB_NOTIFY_TOPIC_FILTER_LEN, &isJob);
    }
    if (isJob) {
        size_t n = payload_len <= sizeof(jobDocBuffer.jobData) ? payload_len : sizeof(jobDocBuffer.jobData);
        memcpy(jobDocBuffer.jobData, payload, n);
        jobDocBuffer.jobDataLength = n;
        OtaEventMsg_t evt = { .jobEvent = &jobDocBuffer, .eventId = OtaAgentEventReceivedJobDocument };
        (void)OtaSendEvent_FreeRTOS(&evt);
    }
}

/* --------------------------- state machine ------------------------------- */
static void processOTAEvents(void)
{
    OtaEventMsg_t recvEvent = { 0 };
    OtaReceiveEvent_FreeRTOS(&recvEvent);

    switch (recvEvent.eventId) {
        case OtaAgentEventRequestJobDocument:
            ESP_LOGI(TAG, "requesting job document");
            requestJobDocumentHandler();
            otaAgentState = OtaAgentStateRequestingJob;
            break;

        case OtaAgentEventReceivedJobDocument:
            if (!receivedJobDocumentHandler(recvEvent.jobEvent)) {
                otaAgentState = OtaAgentStateWaitingForJob;
                break;
            }
            /* HTTP data path: download -> verify -> activate, all inline (the
             * transfer is one streaming GET, not async MQTT blocks). */
            otaAgentState = OtaAgentStateCreatingFile;
            if (http_download_to_pal(&jobFields) != ESP_OK) {
                ESP_LOGE(TAG, "\033[1;31mdownload failed -> FAILED\033[0m");
                reportJobStatus(globalJobId, "FAILED");
                ota_nvs_clear_job_id();
                globalJobId[0] = '\0';
                otaAgentState = OtaAgentStateWaitingForJob;
                break;
            }
            ESP_LOGI(TAG, "verifying signature + closing file");
            if (otaPal_CloseFile(&jobFields) != OtaPalSuccess) {
                ESP_LOGE(TAG, "\033[1;31msignature verification FAILED -> rejecting job\033[0m");
                reportJobStatus(globalJobId, "FAILED");
                ota_nvs_clear_job_id();
                globalJobId[0] = '\0';
                otaAgentState = OtaAgentStateWaitingForJob;
                break;
            }
            ESP_LOGI(TAG, "\033[1;32msignature OK -> activating + rebooting\033[0m");
            if (globalJobId[0] != '\0') {
                ota_nvs_set_job_id(globalJobId);   /* hand-off across the reboot */
            }
            otaPal_ActivateNewImage(&jobFields);   /* sets boot partition + resets */
            otaAgentState = OtaAgentStateStopped;
            break;

        default:
            break;
    }
}

static void otaTask(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1000));   /* let the SUBSCRIBEs settle before StartNext */
    OtaEventMsg_t initEvent = { .eventId = OtaAgentEventRequestJobDocument };
    OtaSendEvent_FreeRTOS(&initEvent);
    for (;;) {
        processOTAEvents();
    }
}

/* --------------------------- trial-boot gate ----------------------------- */
static void resolve_trial_boot(void)
{
    char jobId[MAX_JOB_ID_LENGTH] = { 0 };
    bool haveJob = (ota_nvs_get_job_id(jobId, sizeof(jobId)) == ESP_OK);
    ESP_LOGW(TAG, "trial boot detected — running self-test%s", haveJob ? "" : " (no stashed job id)");

    bool cloud_ok = transport_is_connected();
    bool core_ok  = self_test_core_function_ok();

    if (cloud_ok && core_ok) {
        self_test_commit();
        self_test_disarm_watchdog();
        if (haveJob) {
            reportJobStatus(jobId, "SUCCEEDED");
            strncpy(s_completedJobId, jobId, sizeof(s_completedJobId) - 1);
            ota_nvs_clear_job_id();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
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

static void prvSubscribeJobTopics(void)
{
    char topic[JOBS_API_MAX_LENGTH(MAX_THING_NAME_LEN)] = { 0 };
    size_t len = 0;
    if (Jobs_GetTopic(topic, sizeof(topic), device_iot_thing_name(), strlen(device_iot_thing_name()),
                      JobsNextJobChanged, &len) == JobsSuccess) {
        prvSubscribe(topic, (uint16_t)len, 1);
    }
    len = 0;
    if (Jobs_GetTopic(topic, sizeof(topic), device_iot_thing_name(), strlen(device_iot_thing_name()),
                      JobsStartNextSuccess, &len) == JobsSuccess) {
        prvSubscribe(topic, (uint16_t)len, 1);
    }
}

void ota_backend_on_reconnect(void)
{
    ESP_LOGW(TAG, "reconnected — re-subscribing to job topics");
    prvSubscribeJobTopics();
    OtaEventMsg_t evt = { .eventId = OtaAgentEventRequestJobDocument };
    OtaSendEvent_FreeRTOS(&evt);
}

void ota_backend_start(void)
{
    OtaInitEvent_FreeRTOS();
    otaPal_SetCodeSigningCertificate((const char *)codesign_cert_pem);

    if (self_test_is_trial_boot()) {
        resolve_trial_boot();
    }

    prvSubscribeJobTopics();
    s_ready = true;
    otaAgentState = OtaAgentStateReady;

    if (xTaskCreate(otaTask, "ota", OTA_TASK_STACK_SIZE, NULL, OTA_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create OTA task");
    }
}

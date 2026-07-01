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
#include "ota_jobs_common.h"

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
/* A resolved S3 presigned URL carries an X-Amz-Security-Token (STS), so it runs
 * ~1.2-1.5 KB — 1024 was too small. Stays under JOB_DOC_SIZE (the whole job doc,
 * incl. this URL, must also fit that buffer). */
#define MAX_URL_LEN                2048U

static char     globalJobId[MAX_JOB_ID_LENGTH];
static OtaJobEventData_t    jobDocBuffer;
static AfrOtaJobDocumentFields_t jobFields;
static uint8_t  OtaImageSignatureDecoded[OTA_MAX_SIGNATURE_SIZE];
static volatile bool s_ready;

/* The Jobs control plane (StartNext request, Jobs_Update status report,
 * start-next/accepted subscribe, and the post-commit idempotency guard) is shared
 * across the mqtt/https/jobs backends — see ota_jobs_common.c. */

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
        /* The request line carries the full presigned URL (path + ~1 KB of SigV4
         * query incl. the STS token); the default 512 B tx buffer overflows
         * ("HTTP_CLIENT: Out of buffer"). Size both to hold it + the S3 headers. */
        .buffer_size    = 2048,   /* RX: response headers */
        .buffer_size_tx = 2048,   /* TX: GET request line with the long query */
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
    if (ota_jobs_is_completed(jobId, jobIdLength)) {
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
    /* Validate the download URL BEFORE otaPal_CreateFileForRx (which erases the
     * passive partition) — a job we can't fetch should be a clean no-op. */
    if (jobFields.imageRefLen == 0 || jobFields.imageRefLen >= MAX_URL_LEN) {
        ESP_LOGE(TAG, "presigned URL missing/too long (len=%u, max=%u)",
                 (unsigned)jobFields.imageRefLen, (unsigned)MAX_URL_LEN);
        return false;
    }
    if (otaPal_CreateFileForRx(&jobFields) != OtaPalSuccess) {
        return false;
    }
    ESP_LOGI(TAG, "OTA job accepted (HTTP data path), %lu bytes",
             (unsigned long)jobFields.fileSize);
    ota_jobs_report_status(globalJobId, "IN_PROGRESS");
    return true;
}

/* --------------------------- event routing ------------------------------- */
void ota_backend_on_publish(const char *topic, size_t topic_len,
                                 const uint8_t *payload, size_t payload_len)
{
    if (!s_ready) {
        return;
    }
    /* Deferred-to-boot: only the StartNext response is processed; notify-next pushes
     * are neither subscribed nor handled. */
    bool isJob = Jobs_IsStartNextAccepted(topic, topic_len, device_iot_thing_name(), strlen(device_iot_thing_name()));
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
            ota_jobs_request_document();
            break;

        case OtaAgentEventReceivedJobDocument:
            if (!receivedJobDocumentHandler(recvEvent.jobEvent)) {
                break;
            }
            /* HTTP data path: download -> verify -> activate, all inline (the
             * transfer is one streaming GET, not async MQTT blocks). */
            if (http_download_to_pal(&jobFields) != ESP_OK) {
                ESP_LOGE(TAG, LOG_RED("download failed -> FAILED"));
                ota_jobs_report_status(globalJobId, "FAILED");
                ota_nvs_clear_job_id();
                globalJobId[0] = '\0';
                break;
            }
            ESP_LOGI(TAG, "verifying signature + closing file");
            if (otaPal_CloseFile(&jobFields) != OtaPalSuccess) {
                ESP_LOGE(TAG, LOG_RED("signature verification FAILED -> rejecting job"));
                ota_jobs_report_status(globalJobId, "FAILED");
                ota_nvs_clear_job_id();
                globalJobId[0] = '\0';
                break;
            }
            ESP_LOGI(TAG, LOG_GREEN("signature OK -> activating + rebooting"));
            if (globalJobId[0] != '\0') {
                ota_nvs_set_job_id(globalJobId);   /* hand-off across the reboot */
            }
            otaPal_ActivateNewImage(&jobFields);   /* sets boot partition + resets */
            break;

        default:
            break;
    }
}

static void otaTask(void *arg)
{
    (void)arg;
    /* No initial StartNext here: device_iot drives subscribe+request via the
     * connect path (ota_backend_on_reconnect), so it fires exactly once. */
    for (;;) {
        processOTAEvents();
    }
}

void ota_backend_on_reconnect(void)
{
    ESP_LOGI(TAG, "(re)subscribing to job topics");   /* runs on the first connect too */
    ota_jobs_subscribe_topics();
    OtaEventMsg_t evt = { .eventId = OtaAgentEventRequestJobDocument };
    OtaSendEvent_FreeRTOS(&evt);
}

void ota_backend_start(void)
{
    OtaInitEvent_FreeRTOS();
    otaPal_SetCodeSigningCertificate((const char *)codesign_cert_pem);

    if (self_test_is_trial_boot()) {
        self_test_resolve_trial(ota_jobs_trial_report, 5000);
    }

    s_ready = true;

    if (xTaskCreate(otaTask, "ota", OTA_TASK_STACK_SIZE, NULL, OTA_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create OTA task");
    }
}

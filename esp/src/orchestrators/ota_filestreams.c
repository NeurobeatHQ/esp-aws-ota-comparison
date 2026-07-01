/*
 * ota_orchestrator.c — AWS IoT "Modular OTA" state machine.
 *
 * Adapted from Espressif's ota_over_mqtt_demo.c (which uses coreMQTT-Agent) to
 * plain coreMQTT (see mqtt_client.c), and extended with a real application
 * self-test that gates the post-reboot commit (see self_test.c).
 *
 * Flow:
 *   notify-next / StartNext  ->  AFR_OTA job doc  ->  MQTT file-streams download
 *   ->  ECDSA-P256 signature verify (PAL)  ->  activate + reboot
 *   ->  [trial boot] self-test  ->  commit (SUCCEEDED) or reject (rollback/FAILED)
 *
 * The in-flight job id is stashed in NVS before activation so the freshly booted
 * trial image can report SUCCEEDED/FAILED for it.
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
#include "freertos/semphr.h"
#include "esp_log.h"

#include "core_mqtt.h"          /* MQTT_MatchTopic */
#include "MQTTFileDownloader.h"
#include "MQTTFileDownloader_base64.h"
#include "jobs.h"
#include "job_parser.h"
#include "ota_job_processor.h"
#include "ota_os_freertos.h"
#include "ota_pal.h"

static const char *TAG = "ota";

/* Code-signing certificate (public), embedded as a generated C array (see
 * embed_certs.cmake). The PAL verifies the AWS Signer ECDSA-P256 signature
 * carried in the job doc against it. */
extern const unsigned char codesign_cert_pem[];

/* ------------------------------- tunables -------------------------------- */
#define OTA_MAX_NUM_DATA_BUFFERS   2U
/* Generous: this task runs the coreJSON job-doc parse, the file-streams decode,
 * and the mbedTLS ECDSA-P256 signature verify (otaPal_CloseFile). */
#define OTA_TASK_STACK_SIZE        8192
#define OTA_TASK_PRIORITY          4
#define MAX_JOB_ID_LENGTH          64U
#define OTA_MAX_SIGNATURE_SIZE     384U
#define NUM_OF_BLOCKS_REQUESTED    1U

/* ------------------------------- state ----------------------------------- */
static MqttFileDownloaderContext_t mqttFileDownloaderContext;
static uint32_t numOfBlocksRemaining;
static uint32_t currentBlockOffset;
static uint8_t  currentFileId;
static uint32_t totalBytesReceived;
static char     globalJobId[MAX_JOB_ID_LENGTH];

static OtaDataEvent_t       dataBuffers[OTA_MAX_NUM_DATA_BUFFERS];
static OtaJobEventData_t    jobDocBuffer;
static AfrOtaJobDocumentFields_t jobFields;
static uint8_t              OtaImageSignatureDecoded[OTA_MAX_SIGNATURE_SIZE];

static SemaphoreHandle_t bufferSemaphore;
static volatile bool     s_ready;

/* ------------------------- OTA data buffers ------------------------------ */
static OtaDataEvent_t *getOtaDataEventBuffer(void)
{
    OtaDataEvent_t *freeBuffer = NULL;
    if (xSemaphoreTake(bufferSemaphore, portMAX_DELAY) == pdTRUE) {
        for (uint32_t i = 0; i < OTA_MAX_NUM_DATA_BUFFERS; i++) {
            if (!dataBuffers[i].bufferUsed) {
                dataBuffers[i].bufferUsed = true;
                freeBuffer = &dataBuffers[i];
                break;
            }
        }
        xSemaphoreGive(bufferSemaphore);
    }
    return freeBuffer;
}

static void freeOtaDataEventBuffer(OtaDataEvent_t *buf)
{
    if (xSemaphoreTake(bufferSemaphore, portMAX_DELAY) == pdTRUE) {
        buf->bufferUsed = false;
        xSemaphoreGive(bufferSemaphore);
    }
}

/* The Jobs control plane (StartNext request, Jobs_Update status report,
 * start-next/accepted subscribe, and the post-commit idempotency guard) is shared
 * across the mqtt/https/jobs backends — see ota_jobs_common.c. */

/* ------------------------- job-doc parsing ------------------------------- */
static bool jobDocumentParser(char *message, size_t messageLength,
                              AfrOtaJobDocumentFields_t *fields)
{
    const char *jobDoc;
    size_t jobDocLength = Jobs_GetJobDocument(message, messageLength, &jobDoc);
    int8_t fileIndex = 0;

    if (jobDocLength != 0U) {
        do {
            /* Jobs v1.5.1 (vendored in esp-aws-iot 202406.05-LTS) uses the 4-arg
             * form — MQTT is assumed; no protocol argument. */
            fileIndex = otaParser_parseJobDocFile(jobDoc, jobDocLength, fileIndex, fields);
        } while (fileIndex > 0);
    }
    return fileIndex == 0;   /* 0 = all files parsed, -1 = error */
}

static void initMqttDownloader(AfrOtaJobDocumentFields_t *fields)
{
    numOfBlocksRemaining = fields->fileSize / mqttFileDownloader_CONFIG_BLOCK_SIZE;
    numOfBlocksRemaining += (fields->fileSize % mqttFileDownloader_CONFIG_BLOCK_SIZE > 0) ? 1 : 0;
    currentFileId = (uint8_t)fields->fileId;
    currentBlockOffset = 0;
    totalBytesReceived = 0;

    mqttDownloader_init(&mqttFileDownloaderContext,
                        fields->imageRef, fields->imageRefLen,
                        device_iot_thing_name(), strlen(device_iot_thing_name()), DATA_TYPE_JSON);

    transport_subscribe(mqttFileDownloaderContext.topicStreamData,
                        mqttFileDownloaderContext.topicStreamDataLength, 0);
}

/* AWS delivers the signature as base64; the PAL wants DER. */
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

static int16_t handleMqttStreamsBlockArrived(uint8_t *data, size_t dataLength)
{
    int16_t written = otaPal_WriteBlock(&jobFields, totalBytesReceived, data, dataLength);
    if (written > 0) {
        totalBytesReceived += written;
    }
    return written;
}

static esp_err_t requestDataBlock(void)
{
    char getStreamRequest[GET_STREAM_REQUEST_BUFFER_SIZE];
    size_t len = mqttDownloader_createGetDataBlockRequest(mqttFileDownloaderContext.dataType,
                                                          currentFileId,
                                                          mqttFileDownloader_CONFIG_BLOCK_SIZE,
                                                          (uint16_t)currentBlockOffset,
                                                          NUM_OF_BLOCKS_REQUESTED,
                                                          getStreamRequest,
                                                          GET_STREAM_REQUEST_BUFFER_SIZE);
    return transport_publish(mqttFileDownloaderContext.topicGetStream,
                             mqttFileDownloaderContext.topicGetStreamLength,
                             getStreamRequest, len, 0);
}

static bool closeFileHandler(void)
{
    /* otaPal_CloseFile verifies the ECDSA-P256 signature against the embedded
     * code-signing cert before accepting the image. */
    return otaPal_CloseFile(&jobFields) == OtaPalSuccess;
}

static bool imageActivationHandler(void)
{
    /* Stash the job id so the post-reboot trial image can report its result. */
    if (globalJobId[0] != '\0') {
        ota_nvs_set_job_id(globalJobId);
    }
    /* Sets the boot partition and resets; should not return. */
    return otaPal_ActivateNewImage(&jobFields) == OtaPalSuccess;
}

static OtaPalJobDocProcessingResult_t receivedJobDocumentHandler(OtaJobEventData_t *jobDoc)
{
    const char *jobId;
    size_t jobIdLength = Jobs_GetJobId((char *)jobDoc->jobData, jobDoc->jobDataLength, &jobId);
    OtaPalJobDocProcessingResult_t result = OtaPalJobDocFileCreateFailed;

    if (jobIdLength == 0) {
        return result;   /* not a job message we can act on */
    }

    /* Idempotency guard: if this is the job we just committed during the trial
     * boot, re-affirm SUCCEEDED and do NOT re-download it (the PAL would call
     * esp_ota_begin on the already-valid image otherwise). */
    if (ota_jobs_is_completed(jobId, jobIdLength)) {
        return OtaPalJobDocFileCreateFailed;
    }

    if (jobIdLength >= MAX_JOB_ID_LENGTH) {
        return result;
    }

    memset(&jobFields, 0, sizeof(jobFields));
    memcpy(globalJobId, jobId, jobIdLength);
    globalJobId[jobIdLength] = '\0';

    if (!jobDocumentParser((char *)jobDoc->jobData, jobDoc->jobDataLength, &jobFields)) {
        ESP_LOGI(TAG, "not an OTA job document");
        return result;
    }

    initMqttDownloader(&jobFields);

    if (!convertSignatureToDER(&jobFields)) {
        ESP_LOGE(TAG, "failed to decode image signature");
        return result;
    }

    if (otaPal_CreateFileForRx(&jobFields) == OtaPalSuccess) {
        ESP_LOGI(TAG, "OTA job accepted, starting download (%lu bytes)",
                 (unsigned long)jobFields.fileSize);
        /* Promote the execution QUEUED -> IN_PROGRESS. The notify-next push
         * delivers the doc while the execution is still QUEUED, so without this
         * the cloud job could sit QUEUED and TIME_OUT even on a good image. */
        ota_jobs_report_status(globalJobId, "IN_PROGRESS");
        result = OtaPalJobDocFileCreated;
    }
    return result;
}

/* --------------------------- event routing ------------------------------- */
void ota_backend_on_publish(const char *topic, size_t topic_len,
                                 const uint8_t *payload, size_t payload_len)
{
    if (!s_ready) {
        return;
    }
    OtaEventMsg_t evt = { 0 };

    /* A streamed data block? */
    if (mqttDownloader_isDataBlockReceived(&mqttFileDownloaderContext, topic, topic_len)
            == MQTTFileDownloaderSuccess) {
        OtaDataEvent_t *buf = getOtaDataEventBuffer();
        if (buf == NULL) {
            ESP_LOGW(TAG, "no free OTA data buffer; dropping block");
            return;
        }
        size_t n = payload_len <= sizeof(buf->data) ? payload_len : sizeof(buf->data);
        memcpy(buf->data, payload, n);
        buf->dataLength = n;
        evt.dataEvent = buf;
        evt.eventId = OtaAgentEventReceivedFileBlock;
        if (OtaSendEvent_FreeRTOS(&evt) != OtaOsSuccess) {
            freeOtaDataEventBuffer(buf);
        }
        return;
    }

    /* Deferred-to-boot: only the start-next/accepted job document; notify-next pushes
     * are neither subscribed nor handled. */
    bool isJob = Jobs_IsStartNextAccepted(topic, topic_len, device_iot_thing_name(), strlen(device_iot_thing_name()));
    if (isJob) {
        size_t n = payload_len <= sizeof(jobDocBuffer.jobData) ? payload_len : sizeof(jobDocBuffer.jobData);
        memcpy(jobDocBuffer.jobData, payload, n);
        jobDocBuffer.jobDataLength = n;
        evt.jobEvent = &jobDocBuffer;
        evt.eventId = OtaAgentEventReceivedJobDocument;
        (void)OtaSendEvent_FreeRTOS(&evt);
    }
}

/* --------------------------- state machine ------------------------------- */
static void processOTAEvents(void)
{
    OtaEventMsg_t recvEvent = { 0 };
    OtaEventMsg_t nextEvent = { 0 };

    OtaReceiveEvent_FreeRTOS(&recvEvent);

    switch (recvEvent.eventId) {
        case OtaAgentEventRequestJobDocument:
            ESP_LOGI(TAG, "requesting job document");
            ota_jobs_request_document();
            break;

        case OtaAgentEventReceivedJobDocument:
            switch (receivedJobDocumentHandler(recvEvent.jobEvent)) {
                case OtaPalJobDocFileCreated:
                    nextEvent.eventId = OtaAgentEventRequestFileBlock;
                    OtaSendEvent_FreeRTOS(&nextEvent);
                    break;
                default:
                    /* Not an actionable OTA job; keep waiting for the next one. */
                    break;
            }
            break;

        case OtaAgentEventRequestFileBlock:
            if (currentBlockOffset == 0) {
                ESP_LOGI(TAG, "starting download");
            }
            if (requestDataBlock() != ESP_OK) {
                ESP_LOGE(TAG, "block request failed; retrying");
                nextEvent.eventId = OtaAgentEventRequestFileBlock;
                OtaSendEvent_FreeRTOS(&nextEvent);
            }
            break;

        case OtaAgentEventReceivedFileBlock: {
            static uint8_t decoded[mqttFileDownloader_CONFIG_BLOCK_SIZE];
            size_t decodedLen = 0;
            int32_t fileId = 0, blockId = 0, blockSize = 0;
            static int32_t lastBlockId = -1;
            int16_t written = -1;

            MQTTFileDownloaderStatus_t st =
                mqttDownloader_processReceivedDataBlock(&mqttFileDownloaderContext,
                                                        recvEvent.dataEvent->data,
                                                        recvEvent.dataEvent->dataLength,
                                                        &fileId, &blockId, &blockSize,
                                                        decoded, &decodedLen);
            if (st == MQTTFileDownloaderSuccess &&
                fileId == (int32_t)jobFields.fileId &&
                blockSize <= (int32_t)mqttFileDownloader_CONFIG_BLOCK_SIZE &&
                blockId > lastBlockId) {
                written = handleMqttStreamsBlockArrived(decoded, decodedLen);
                lastBlockId = blockId;
            }
            freeOtaDataEventBuffer(recvEvent.dataEvent);

            if (written > 0) {
                numOfBlocksRemaining--;
                currentBlockOffset++;
            }
            if (numOfBlocksRemaining == 0) {
                ESP_LOGI(TAG, "download complete (%lu bytes)", (unsigned long)totalBytesReceived);
                lastBlockId = -1;
                nextEvent.eventId = OtaAgentEventCloseFile;
            } else {
                nextEvent.eventId = OtaAgentEventRequestFileBlock;
            }
            OtaSendEvent_FreeRTOS(&nextEvent);
            break;
        }

        case OtaAgentEventCloseFile:
            ESP_LOGI(TAG, "verifying signature + closing file");
            if (closeFileHandler()) {
                ESP_LOGI(TAG, LOG_GREEN("signature OK -> activating new image"));
                nextEvent.eventId = OtaAgentEventActivateImage;
                OtaSendEvent_FreeRTOS(&nextEvent);
            } else {
                ESP_LOGE(TAG, LOG_RED("signature verification FAILED -> rejecting job"));
                ota_jobs_report_status(globalJobId, "FAILED");
                ota_nvs_clear_job_id();
                globalJobId[0] = '\0';
            }
            break;

        case OtaAgentEventActivateImage:
            ESP_LOGI(TAG, "activating + rebooting into new image...");
            imageActivationHandler();   /* reboots; should not return */
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

/* Fired by the MQTT task after a cleanSession reconnect: re-subscribe and ask
 * for the current job again. Runs in the MQTT task; only does thread-safe
 * subscribe + a queue post. */
void ota_backend_on_reconnect(void)
{
    ESP_LOGI(TAG, "(re)subscribing to job topics");   /* runs on the first connect too */
    ota_jobs_subscribe_topics();
    OtaEventMsg_t evt = { .eventId = OtaAgentEventRequestJobDocument };
    OtaSendEvent_FreeRTOS(&evt);
}

void ota_backend_start(void)
{
    bufferSemaphore = xSemaphoreCreateMutex();
    configASSERT(bufferSemaphore != NULL);
    memset(dataBuffers, 0, sizeof(dataBuffers));

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

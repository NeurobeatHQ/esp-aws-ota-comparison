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

#define OTA_TOPIC_PREFIX                   "$aws/things/+/"
#define OTA_JOB_NOTIFY_TOPIC_FILTER        OTA_TOPIC_PREFIX "jobs/notify-next"
#define OTA_JOB_NOTIFY_TOPIC_FILTER_LEN    ((uint16_t)(sizeof(OTA_JOB_NOTIFY_TOPIC_FILTER) - 1))

/* OTA MQTT control/status return status (mirrors the reference's local enum). */
typedef enum { OtaMqttSuccess = 0, OtaMqttPublishFailed, OtaMqttSubscribeFailed } OtaMqttStatus_t;

/* ------------------------------- state ----------------------------------- */
static MqttFileDownloaderContext_t mqttFileDownloaderContext;
static uint32_t numOfBlocksRemaining;
static uint32_t currentBlockOffset;
static uint8_t  currentFileId;
static uint32_t totalBytesReceived;
static char     globalJobId[MAX_JOB_ID_LENGTH];
static char     s_completedJobId[MAX_JOB_ID_LENGTH];   /* guards re-processing post-commit */

static OtaDataEvent_t       dataBuffers[OTA_MAX_NUM_DATA_BUFFERS];
static OtaJobEventData_t    jobDocBuffer;
static AfrOtaJobDocumentFields_t jobFields;
static uint8_t              OtaImageSignatureDecoded[OTA_MAX_SIGNATURE_SIZE];

static OtaState_t        otaAgentState = OtaAgentStateInit;
static SemaphoreHandle_t bufferSemaphore;
static volatile bool     s_ready;

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

/* Build the Jobs Update topic for jobId and publish {"status":"<status>"}. */
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

    prvSubscribe(mqttFileDownloaderContext.topicStreamData,
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

static OtaMqttStatus_t requestDataBlock(void)
{
    char getStreamRequest[GET_STREAM_REQUEST_BUFFER_SIZE];
    size_t len = mqttDownloader_createGetDataBlockRequest(mqttFileDownloaderContext.dataType,
                                                          currentFileId,
                                                          mqttFileDownloader_CONFIG_BLOCK_SIZE,
                                                          (uint16_t)currentBlockOffset,
                                                          NUM_OF_BLOCKS_REQUESTED,
                                                          getStreamRequest,
                                                          GET_STREAM_REQUEST_BUFFER_SIZE);
    return prvPublish(mqttFileDownloaderContext.topicGetStream,
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
    if (s_completedJobId[0] != '\0' &&
        strlen(s_completedJobId) == jobIdLength &&            /* full equality, not prefix */
        strncmp(s_completedJobId, jobId, jobIdLength) == 0) {
        ESP_LOGI(TAG, "ignoring already-completed job (re-affirming SUCCEEDED)");
        reportJobStatus(s_completedJobId, "SUCCEEDED");
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
        reportJobStatus(globalJobId, "IN_PROGRESS");
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

    /* The start-next/accepted job document, or a notify-next push? */
    bool isJob = Jobs_IsStartNextAccepted(topic, topic_len, device_iot_thing_name(), strlen(device_iot_thing_name()));
    if (!isJob) {
        (void)MQTT_MatchTopic(topic, topic_len, OTA_JOB_NOTIFY_TOPIC_FILTER,
                              OTA_JOB_NOTIFY_TOPIC_FILTER_LEN, &isJob);
    }
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
            requestJobDocumentHandler();
            otaAgentState = OtaAgentStateRequestingJob;
            break;

        case OtaAgentEventReceivedJobDocument:
            switch (receivedJobDocumentHandler(recvEvent.jobEvent)) {
                case OtaPalJobDocFileCreated:
                    nextEvent.eventId = OtaAgentEventRequestFileBlock;
                    OtaSendEvent_FreeRTOS(&nextEvent);
                    otaAgentState = OtaAgentStateCreatingFile;
                    break;
                default:
                    /* Not an actionable OTA job; keep waiting for the next one. */
                    otaAgentState = OtaAgentStateWaitingForJob;
                    break;
            }
            break;

        case OtaAgentEventRequestFileBlock:
            otaAgentState = OtaAgentStateRequestingFileBlock;
            if (currentBlockOffset == 0) {
                ESP_LOGI(TAG, "starting download");
            }
            if (requestDataBlock() != OtaMqttSuccess) {
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
                ESP_LOGI(TAG, "\033[1;32msignature OK -> activating new image\033[0m");
                nextEvent.eventId = OtaAgentEventActivateImage;
                OtaSendEvent_FreeRTOS(&nextEvent);
            } else {
                ESP_LOGE(TAG, "\033[1;31msignature verification FAILED -> rejecting job\033[0m");
                reportJobStatus(globalJobId, "FAILED");
                ota_nvs_clear_job_id();
                globalJobId[0] = '\0';
                otaAgentState = OtaAgentStateWaitingForJob;
            }
            break;

        case OtaAgentEventActivateImage:
            ESP_LOGI(TAG, "activating + rebooting into new image...");
            imageActivationHandler();   /* reboots; should not return */
            otaAgentState = OtaAgentStateStopped;
            break;

        default:
            break;
    }
}

static void otaTask(void *arg)
{
    (void)arg;
    /* Let the job-topic SUBSCRIBEs be acknowledged before the first StartNext so
     * its start-next/accepted response isn't published before we're listening. */
    vTaskDelay(pdMS_TO_TICKS(1000));
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

    ESP_LOGW(TAG, "trial boot detected — running self-test%s",
             haveJob ? "" : " (no stashed job id)");

    bool cloud_ok = transport_is_connected();
    bool core_ok  = self_test_core_function_ok();

    if (cloud_ok && core_ok) {
        self_test_commit();              /* esp_ota_mark_app_valid_cancel_rollback */
        self_test_disarm_watchdog();
        if (haveJob) {
            reportJobStatus(jobId, "SUCCEEDED");
            strncpy(s_completedJobId, jobId, sizeof(s_completedJobId) - 1);
            ota_nvs_clear_job_id();
        }
        /* Give IoT Core a moment to mark the job SUCCEEDED before StartNext. */
        vTaskDelay(pdMS_TO_TICKS(5000));
    } else {
        ESP_LOGE(TAG, "self-test failed (cloud=%d core=%d) -> rollback", cloud_ok, core_ok);
        if (haveJob) {
            reportJobStatus(jobId, "FAILED");
            ota_nvs_clear_job_id();
            vTaskDelay(pdMS_TO_TICKS(1500));   /* best-effort flush before reboot */
        }
        self_test_reject_and_rollback();       /* does not return */
    }
}

/* Subscribe to both job-control topics:
 *   - jobs/notify-next         : push when the next job changes (job pushed
 *                                while we're connected)
 *   - jobs/start-next/accepted : response to the StartNext we publish (claims a
 *                                job already queued at connect / after reboot)
 * Both are needed: notify-next only fires for the FIRST queued execution, so a
 * device that reconnects with a pending job relies on the StartNext path. */
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

/* Fired by the MQTT task after a cleanSession reconnect: re-subscribe and ask
 * for the current job again. Runs in the MQTT task; only does thread-safe
 * subscribe + a queue post. */
void ota_backend_on_reconnect(void)
{
    ESP_LOGI(TAG, "(re)subscribing to job topics");   /* runs on the first connect too */
    prvSubscribeJobTopics();
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
        resolve_trial_boot();
    }

    prvSubscribeJobTopics();

    s_ready = true;
    otaAgentState = OtaAgentStateReady;

    if (xTaskCreate(otaTask, "ota", OTA_TASK_STACK_SIZE, NULL, OTA_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "failed to create OTA task");
    }
}

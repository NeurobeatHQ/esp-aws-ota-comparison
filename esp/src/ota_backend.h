/*
 * ota_backend.h — the common OTA-control-plane contract.
 *
 * Four implementations exist; exactly one is compiled per build env:
 *   orchestrators/ota_filestreams.c  (mqtt:  AWS OTA agent, MQTT File Streams, Signer verify)
 *   orchestrators/ota_http.c         (https: AWS OTA agent, HTTPS data path,   Signer verify)
 *   orchestrators/ota_jobs.c         (jobs:  AWS IoT Jobs lib + esp_https_ota)
 *   orchestrators/ota_manual.c       (manual:custom MQTT protocol + esp_https_ota)
 *
 * device_iot drives these; the application never calls them directly.
 */
#ifndef OTA_BACKEND_H
#define OTA_BACKEND_H

#include <stddef.h>
#include <stdint.h>

/* Resolve a post-OTA trial boot (self-test -> commit/rollback) and start the
 * background OTA control plane. Called once after the transport is up. */
void ota_backend_start(void);

/* Fed every incoming PUBLISH by device_iot's router so the backend can pick out
 * its own control/data topics ($aws/things/.../jobs|streams or dt/.../ota/...). */
void ota_backend_on_publish(const char *topic, size_t topic_len,
                            const uint8_t *payload, size_t payload_len);

/* Called by device_iot after a transport reconnect so the backend re-subscribes
 * to its control topics (the broker forgets them across a cleanSession reconnect)
 * and re-arms its control plane. device_iot owns the single transport conn cb. */
void ota_backend_on_reconnect(void);

#endif /* OTA_BACKEND_H */

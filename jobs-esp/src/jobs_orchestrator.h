/*
 * jobs_orchestrator.h — variant B control plane: AWS IoT Jobs over esp-mqtt.
 *
 * Uses the standalone Jobs lib's topic helpers; parses a CUSTOM job document
 * {"op":"ota","url":<presigned-S3>,"target_version":...} with cJSON; downloads
 * via esp_https_ota; commits/rolls back via self_test (identical to variant C).
 */
#ifndef JOBS_ORCHESTRATOR_H
#define JOBS_ORCHESTRATOR_H

#include <stddef.h>
#include <stdint.h>

/* Call once after MQTT is connected: resolve a trial boot, subscribe to the job
 * topics, and start the orchestrator task. */
void jobs_orchestrator_start(void);

/* Registered with mqtt_es as the incoming-PUBLISH handler. */
void jobs_orchestrator_on_publish(const char *topic, size_t topic_len,
                                  const uint8_t *payload, size_t payload_len);

#endif /* JOBS_ORCHESTRATOR_H */

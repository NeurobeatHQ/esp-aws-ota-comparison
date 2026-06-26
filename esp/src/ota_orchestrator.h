/*
 * ota_orchestrator.h — AWS IoT "Modular OTA" state machine.
 *
 * Drives the AWS IoT Jobs library + the MQTT File Streams downloader + the
 * ESP32 OTA PAL over a plain coreMQTT connection. Adapted from Espressif's
 * ota_over_mqtt_demo.c (coreMQTT-Agent) to plain coreMQTT, with a real
 * application self-test gating the post-reboot commit.
 */
#ifndef OTA_ORCHESTRATOR_H
#define OTA_ORCHESTRATOR_H

#include <stddef.h>
#include <stdint.h>

/* Call once after MQTT is connected. Resolves a trial boot (self-test ->
 * commit or reject/rollback), subscribes to the Jobs topics, and starts the
 * OTA task that services current and future OTA jobs. */
void ota_orchestrator_start(void);

/* Registered with mqtt_client as the incoming-PUBLISH handler. Routes Jobs and
 * stream messages into the OTA event queue. Cheap + non-blocking. */
void ota_orchestrator_on_publish(const char *topic, size_t topic_len,
                                 const uint8_t *payload, size_t payload_len);

#endif /* OTA_ORCHESTRATOR_H */

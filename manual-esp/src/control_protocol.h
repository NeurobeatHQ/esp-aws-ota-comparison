/*
 * control_protocol.h — variant A control plane: a fully custom MQTT
 * request/response OTA protocol (no AWS IoT Jobs).
 *
 *   device -> dt/<thing>/ota/should-i-update   {"thing","version"}
 *   server -> dt/<thing>/ota/plan              {"op":"ota","url","ota_id","target_version"} | {"op":"none"}
 *   device -> dt/<thing>/ota/confirm           {"ota_id","result":"success|failed","version"}
 *
 * Download via esp_https_ota; commit/rollback via self_test (identical to C).
 */
#ifndef CONTROL_PROTOCOL_H
#define CONTROL_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

void control_protocol_start(void);
void control_protocol_on_publish(const char *topic, size_t topic_len,
                                 const uint8_t *payload, size_t payload_len);

#endif /* CONTROL_PROTOCOL_H */

/*
 * mqtt_client.h — coreMQTT over mutual TLS (esp-tls network_transport).
 *
 * A dedicated task owns MQTT_ProcessLoop and delivers incoming PUBLISHes to the
 * registered callback. Other tasks publish/subscribe through the thread-safe
 * wrappers below; a mutex serialises all access to the shared MQTT context.
 */
#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Called from the MQTT task for every incoming application PUBLISH. The handler
 * must be cheap and non-blocking and MUST NOT call back into coreMQTT (copy the
 * payload and hand it off to another task). */
typedef void (*mqtt_incoming_publish_cb_t)(const char *topic,
                                            size_t topic_len,
                                            const uint8_t *payload,
                                            size_t payload_len);

/* Establish the TLS+MQTT connection to AWS IoT Core and start the process-loop
 * task (with auto-reconnect). Blocks until the first CONNACK or hard failure. */
esp_err_t mqtt_client_start(mqtt_incoming_publish_cb_t cb);

/* Invoked from the MQTT task after every *re*connect (cleanSession drops the
 * subscriptions, so the orchestrator re-subscribes here). Not fired for the
 * initial connect established by mqtt_client_start(). */
typedef void (*mqtt_reconnect_cb_t)(void);
void mqtt_client_set_reconnect_cb(mqtt_reconnect_cb_t cb);

bool mqtt_client_is_connected(void);

/* Thread-safe publish/subscribe. OTA control traffic uses QoS 0, so publishes
 * complete without an inter-task ack wait. */
esp_err_t mqtt_client_publish(const char *topic, uint16_t topic_len,
                              const void *payload, size_t payload_len, uint8_t qos);
esp_err_t mqtt_client_subscribe(const char *topic_filter, uint16_t filter_len, uint8_t qos);

#endif /* MQTT_CLIENT_H */

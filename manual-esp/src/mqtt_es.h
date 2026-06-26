/*
 * mqtt_es.h — esp-mqtt mutual-TLS wrapper (variants A and B share this file).
 *
 * Presents the same surface as variant C's coreMQTT mqtt_client.h, so the
 * control-plane code is transport-agnostic. Internally uses the IDF-native
 * esp-mqtt client and reassembles chunked MQTT_EVENT_DATA before delivery.
 */
#ifndef MQTT_ES_H
#define MQTT_ES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Called (from the esp-mqtt task) for each fully-reassembled incoming PUBLISH.
 * Cheap + non-blocking; copy the payload and hand off to another task. */
typedef void (*mqtt_es_publish_cb_t)(const char *topic, size_t topic_len,
                                     const uint8_t *payload, size_t payload_len);

/* Fired (from the esp-mqtt task) after every *re*connect — re-subscribe here.
 * Not fired for the initial connect established by mqtt_es_start(). */
typedef void (*mqtt_es_reconnect_cb_t)(void);

/* Start esp-mqtt to AWS IoT Core (mutual TLS). esp-mqtt auto-reconnects in the
 * background. Blocks until the first CONNECTED event or a timeout. */
esp_err_t mqtt_es_start(mqtt_es_publish_cb_t cb);
void mqtt_es_set_reconnect_cb(mqtt_es_reconnect_cb_t cb);
bool mqtt_es_is_connected(void);

/* Thread-safe publish/subscribe. Topics need not be NUL-terminated (the wrapper
 * copies + terminates). OTA control traffic uses QoS 0/1. */
esp_err_t mqtt_es_publish(const char *topic, uint16_t topic_len,
                          const void *payload, size_t payload_len, uint8_t qos);
esp_err_t mqtt_es_subscribe(const char *topic_filter, uint16_t filter_len, uint8_t qos);

#endif /* MQTT_ES_H */

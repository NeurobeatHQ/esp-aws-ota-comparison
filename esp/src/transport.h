/*
 * transport.h — the common MQTT transport contract.
 *
 * Two implementations exist; exactly one is compiled per build env:
 *   transports/transport_coremqtt.c  (coreMQTT, used by the mqtt + https backends)
 *   transports/transport_espmqtt.c   (esp-mqtt, used by the jobs + manual backends)
 *
 * Nothing above this layer (device_iot, the app) knows which is linked.
 */
#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Invoked for every incoming PUBLISH (topic is NUL-terminated). */
typedef void (*transport_publish_cb_t)(const char *topic, size_t topic_len,
                                       const uint8_t *payload, size_t payload_len);
/* Connection-state change, fired exactly once per transition: up=true on EVERY
 * (re)connect including the first, up=false on a disconnect. The owner
 * (device_iot) re-subscribes and announces the app on up=true. */
typedef void (*transport_conn_cb_t)(bool up);

/* Resolved connection config. device_iot fills endpoint/port/thing_name with
 * non-NULL/non-zero values (already defaulted); the cert PEMs may be NULL, in
 * which case the transport uses its build-embedded cert/key. The pointed-to
 * strings must outlive the connection. */
typedef struct {
    const char *endpoint;
    uint16_t    port;
    const char *thing_name;
    const char *root_ca_pem;       /* server root CA PEM (embedded, public) */
    const char *client_cert_pem;   /* device cert PEM (from the esp_secure_cert partition) */
    const char *client_key_pem;    /* device key PEM (NULL iff use_secure_element / DS) */
    bool        use_secure_element;
    void       *ds_data;
} transport_config_t;

esp_err_t transport_start(transport_publish_cb_t cb, const transport_config_t *cfg);
void      transport_set_conn_cb(transport_conn_cb_t cb);
bool      transport_is_connected(void);

/* Publish — uniform contract on BOTH transports: NON-BLOCKING, enqueues into a
 * bounded outbox and returns. QoS1 messages are retransmitted from the outbox
 * until acknowledged. Returns ESP_OK if accepted (NOT delivery-confirmed),
 * ESP_ERR_NO_MEM if the outbox is full (caller decides drop vs retry),
 * ESP_ERR_INVALID_STATE if never yet connected. */
esp_err_t transport_publish(const char *topic, uint16_t topic_len,
                            const void *payload, size_t payload_len, uint8_t qos);
esp_err_t transport_subscribe(const char *topic_filter, uint16_t filter_len, uint8_t qos);

#endif /* TRANSPORT_H */

/*
 * device_iot.h — the stable, backend-agnostic device API.
 *
 * This is the ONLY header an application includes. The same app code compiles
 * and runs on any of the four OTA backends (mqtt / https / jobs / manual) —
 * pick one at build time with `pio run -e <backend>`. Nothing here changes when
 * you swap the backend.
 */
#ifndef DEVICE_IOT_H
#define DEVICE_IOT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Connection identity + endpoint, supplied explicitly at init. Populate every
 * field (use device_iot_default_config() for the build-time identity, then
 * override individual fields to provision per device). The string buffers (PEM,
 * endpoint) must outlive the connection: esp-tls / coreMQTT hold the cert pointers
 * by reference (endpoint + thing_name are copied internally). */
typedef struct {
    const char *endpoint;          /* AWS IoT ATS endpoint host */
    uint16_t    port;              /* broker port, usually 8883 */
    const char *thing_name;        /* clientId + dt/<thing>/ + $aws/things/<thing>/ */
    const char *root_ca_pem;       /* NUL-terminated PEM: server root CA */
    const char *client_cert_pem;   /* NUL-terminated PEM: device cert */
    const char *client_key_pem;    /* NUL-terminated PEM: device key (NULL iff use_secure_element) */
    bool        use_secure_element;/* private key lives in the DS peripheral / esp_secure_cert */
    void       *ds_data;           /* DS context when use_secure_element */
} device_iot_config_t;

/* Fill cfg with the BUILD-TIME identity: the secrets.h endpoint/port/Thing name and
 * the certs embedded by embed_certs.cmake. Use it as-is, or as a base and override
 * fields (e.g. cfg.thing_name = serial; cfg.client_cert_pem = cert_from_nvs) before
 * device_iot_init(). This makes the defaults explicit instead of hidden behind a
 * NULL sentinel. */
void device_iot_default_config(device_iot_config_t *cfg);

/* Bring up everything: NVS, Wi-Fi, the mutual-TLS cloud connection, the OTA
 * backend, and resolve a post-OTA trial boot. Call once from app_main() with a
 * fully-populated config (see device_iot_default_config); cfg must be non-NULL with
 * at least endpoint, thing_name, and the certs set, else ESP_ERR_INVALID_ARG. */
esp_err_t device_iot_init(const device_iot_config_t *cfg);

/* Is the cloud connection up? */
bool device_iot_is_connected(void);

/* Message callback: receives the FULL topic (NUL-terminated) plus payload. */
typedef void (*device_iot_msg_cb_t)(const char *topic, const uint8_t *data, size_t len);

/* Publish to dt/<thing>/<subtopic> (convenience namespace).
 *
 * NON-BLOCKING on every backend: the message is enqueued into a bounded outbox
 * and sent by the transport task. Backpressure is uniform — returns ESP_OK if
 * accepted (which is NOT delivery confirmation), or ESP_ERR_NO_MEM if the outbox
 * is full (you decide drop vs retry). qos=1 is delivered at-least-once:
 * retransmitted from the outbox until the broker PUBACKs it (survives reconnects).
 * A qos=1 message left unacknowledged for ~70 s (the bounded-buffer fail-safe) is
 * then dropped with a log line — real broker latency won't trip it, but a long
 * outage can, so a critical message warrants an app-level confirm.
 * The same call has the same semantics whether coreMQTT or esp-mqtt is linked. */
esp_err_t device_iot_publish(const char *subtopic, const void *data, size_t len, int qos);

/* Subscribe to dt/<thing>/<subtopic>; cb receives the full topic. */
esp_err_t device_iot_subscribe(const char *subtopic, device_iot_msg_cb_t cb);

/* Escape hatch: publish/subscribe to an ARBITRARY topic (Device Shadow
 * $aws/things/<thing>/shadow/..., a fleet topic, any hierarchy). Same outbox /
 * QoS / backpressure contract as device_iot_publish. The subscribe filter may use
 * MQTT wildcards (+ / #). These share the one connection with the OTA backend. */
esp_err_t device_iot_publish_topic(const char *topic, const void *data, size_t len, int qos);
esp_err_t device_iot_subscribe_topic(const char *filter, device_iot_msg_cb_t cb);

/* Connection-lifecycle hook. Fires connected=true on the first connect AND after
 * every reconnect (the facade has already re-subscribed your subscriptions by
 * then, so just (re)publish birth/state here), and connected=false on a drop.
 * Register before device_iot_init(). May run in the transport task context. */
typedef void (*device_iot_conn_cb_t)(bool connected);
void device_iot_set_connection_cb(device_iot_conn_cb_t cb);

/* Register the application's post-OTA health check BEFORE device_iot_init().
 * On a trial boot the backend calls it to decide commit vs rollback. If unset,
 * the build-time demo default (FW_SELFTEST_SHOULD_PASS) is used. */
typedef bool (*device_iot_health_cb_t)(void);
void device_iot_set_health_check(device_iot_health_cb_t cb);

/* The compiled-in backend name: "mqtt" | "https" | "jobs" | "manual". */
const char *device_iot_backend_name(void);

/* The resolved Thing name (the config override, or the build-time THING_NAME).
 * Valid after device_iot_init(); the OTA backend builds $aws/things/<thing>/...
 * from this so its control plane matches the connection's clientId. */
const char *device_iot_thing_name(void);

#endif /* DEVICE_IOT_H */

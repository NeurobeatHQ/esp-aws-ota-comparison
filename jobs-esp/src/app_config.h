/*
 * app_config.h — variant B (jobs-esp). Central non-secret configuration.
 * Secrets (Wi-Fi, endpoint, Thing name) live in secrets.h (gitignored).
 * Device cert/key + AWS root CA are embedded from src/certs (embed_certs.cmake).
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "secrets.h"

/* Firmware version + self-test variant (overridable on the command line; the
 * fixture script passes -D... via PLATFORMIO_BUILD_FLAGS).
 *   vGOOD: FW_SELFTEST_SHOULD_PASS=1  -> commits;  vBAD: =0 -> rolls back. */
#ifndef APP_VERSION_MAJOR
    #define APP_VERSION_MAJOR    1
#endif
#ifndef APP_VERSION_MINOR
    #define APP_VERSION_MINOR    0
#endif
#ifndef APP_VERSION_BUILD
    #define APP_VERSION_BUILD    0
#endif
#ifndef FW_SELFTEST_SHOULD_PASS
    #define FW_SELFTEST_SHOULD_PASS    1
#endif

/* Connection — AWS IoT Core (mutual TLS) for the Jobs control plane. The
 * firmware data path is HTTPS to a presigned S3 URL (esp_https_ota). */
#ifndef AWS_IOT_ENDPOINT
    #error "Define AWS_IOT_ENDPOINT in secrets.h (xxxx-ats.iot.<region>.amazonaws.com)"
#endif
#ifndef AWS_MQTT_PORT
    #define AWS_MQTT_PORT    8883
#endif
#ifndef THING_NAME
    #error "Define THING_NAME in secrets.h (must equal the AWS IoT Thing name)"
#endif

/* Tunables */
#define WIFI_MAXIMUM_RETRY              10
#define MQTT_KEEP_ALIVE_SECONDS         60
/* esp-mqtt rx/tx buffer + our chunk-reassembly buffer; must hold a job doc. */
#define MQTT_NETWORK_BUFFER_SIZE        8192
#define MQTT_INITIAL_CONNECT_TIMEOUT_MS ( 60 * 1000 )

/* Self-test window (covers Wi-Fi + MQTT + core check; a hang -> WDT -> rollback). */
#define SELF_TEST_WATCHDOG_TIMEOUT_MS   ( 180 * 1000 )

/* NVS hand-off of the in-flight job id across the OTA reboot. */
#define OTA_NVS_NAMESPACE               "ota"
#define OTA_NVS_KEY_JOB_ID              "job_id"

#endif /* APP_CONFIG_H */

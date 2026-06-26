/*
 * app_config.h — central, non-secret configuration for the ESP32-S3 OTA POC.
 *
 * Secrets (Wi-Fi creds, the AWS IoT endpoint, the Thing name) live in secrets.h,
 * which is .gitignored. Copy secrets.h.example -> secrets.h and fill it in.
 *
 * The device cert/key + AWS root CA + code-signing cert are embedded from the
 * src/certs directory (see src/CMakeLists.txt EMBED_TXTFILES).
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "secrets.h"

/* --------------------------------------------------------------------------
 * Firmware version + self-test variant.
 *
 * Defaults are behind #ifndef so a build can override them on the command line
 * (the fixture script passes -D... via PLATFORMIO_BUILD_FLAGS) without a
 * redefinition clash:
 *
 *   vGOOD : FW_SELFTEST_SHOULD_PASS=1  -> self-test passes, OTA commits
 *   vBAD  : FW_SELFTEST_SHOULD_PASS=0  -> self-test fails, device rolls back
 * ------------------------------------------------------------------------ */
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

/* --------------------------------------------------------------------------
 * Connection.  The same firmware always talks to AWS IoT Core for the OTA
 * control plane (Jobs) + data plane (MQTT file streams). The transport keeps
 * endpoint + server-trust as data (network_transport's NetworkContext), so an
 * alternate broker/trust anchor is a config change, not a protocol change.
 * ------------------------------------------------------------------------ */
#ifndef AWS_IOT_ENDPOINT
    #error "Define AWS_IOT_ENDPOINT in secrets.h (xxxx-ats.iot.<region>.amazonaws.com)"
#endif
#ifndef AWS_MQTT_PORT
    #define AWS_MQTT_PORT    8883
#endif

/* The MQTT client id MUST equal the Thing name (the device IoT policy pins
 * iot:Connect to clientId == ${iot:Connection.Thing.ThingName}, and the OTA
 * topic builders derive $aws/things/<thing>/... from it). */
#ifndef THING_NAME
    #error "Define THING_NAME in secrets.h (must equal the AWS IoT Thing name)"
#endif

/* --------------------------------------------------------------------------
 * Tunables
 * ------------------------------------------------------------------------ */
#define WIFI_MAXIMUM_RETRY              10
#define MQTT_KEEP_ALIVE_SECONDS         60
#define MQTT_NETWORK_BUFFER_SIZE        8192   /* must hold a job doc + headers */
#define MQTT_PROCESS_LOOP_TIMEOUT_MS    500

/* Self-test window. Covers Wi-Fi + the (bounded) initial MQTT connect + the
 * core-function check. If the trial image HANGS within this budget the Task WDT
 * panics -> reboot -> the uncommitted image is rolled back. (The bootloader RTC
 * WDT only guards pre-app_main boot; IDF disables it during system init when
 * BOOTLOADER_WDT_DISABLE_IN_USER_CODE=n, so this app-armed WDT is the self-test
 * guard.) Explicit failures (no Wi-Fi, no cloud, bad core check) reject and roll
 * back sooner than this. */
#define SELF_TEST_WATCHDOG_TIMEOUT_MS   ( 180 * 1000 )

/* Bound the *initial* connect so a trial image that cannot reach the cloud fails
 * its self-test promptly (-> rollback) instead of retrying forever. The
 * background MQTT task reconnects indefinitely once we are past the self-test. */
#define MQTT_INITIAL_CONNECT_ATTEMPTS   8

/* NVS namespace/key used to carry the in-flight OTA job id across the reboot,
 * so the trial image can report SUCCEEDED/FAILED for it. */
#define OTA_NVS_NAMESPACE               "ota"
#define OTA_NVS_KEY_JOB_ID              "job_id"

#endif /* APP_CONFIG_H */

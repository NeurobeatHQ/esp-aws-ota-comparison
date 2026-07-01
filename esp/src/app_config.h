/*
 * app_config.h — central, non-secret configuration for the ESP32-S3 OTA POC.
 *
 * Secrets (Wi-Fi creds, the AWS IoT endpoint) live in secrets.h, which is
 * .gitignored. Copy secrets.h.example -> secrets.h and fill it in.
 *
 * The AWS root CA + code-signing cert (both public) are embedded from the
 * src/certs directory at configure time (src/CMakeLists.txt runs
 * ../embed_certs.cmake via execute_process). The device cert/key are NOT
 * embedded — they come from the on-flash esp_secure_cert partition.
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
 * Onboard LED — a version-pattern indicator (src/led.c).
 * The LED blinks APP_VERSION_MAJOR short pulses per cycle, so each firmware
 * version looks different on the board: v1 = 1 blink, v2 = 2 blinks, ... You can
 * then watch a fleet OTA-upgrade by eye (and a rollback as 1->2->1).
 * LED_GPIO 48 matches the board this was tested on; the Adafruit Feather
 * ESP32-S3's own LEDs are GPIO 13 (red) / GPIO 33 (NeoPixel) — change it here if
 * 48 doesn't light. Set LED_ACTIVE_HIGH 0 if your LED lights on a LOW level.
 * ------------------------------------------------------------------------ */
#ifndef LED_GPIO
    #define LED_GPIO    48
#endif
#ifndef LED_ACTIVE_HIGH
    #define LED_ACTIVE_HIGH    1
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
 * topic builders derive $aws/things/<thing>/... from it). It is read at runtime
 * from the device cert's subject CN (esp_secure_cert), never compiled in. */

/* Upper bound for the runtime Thing name (the cert CN). Bounds the resolved copy
 * in device_iot and the OTA topic buffers in the orchestrators. AWS IoT permits
 * up to 128 chars; this covers it. */
#define MAX_THING_NAME_LEN 128

/* --------------------------------------------------------------------------
 * Tunables
 * ------------------------------------------------------------------------ */
#define MQTT_KEEP_ALIVE_SECONDS         60
#define MQTT_NETWORK_BUFFER_SIZE        8192   /* must hold a job doc + headers */

/* Boot connectivity policy (device_iot). A NORMAL boot waits briefly for Wi-Fi
 * then runs offline (Wi-Fi + the transport keep reconnecting in the background) —
 * the app never blocks on connectivity. A TRIAL boot must reach Wi-Fi AND the
 * cloud within these budgets or it rolls back (the new image must prove it can
 * phone home). Both trial budgets sit under SELF_TEST_WATCHDOG_TIMEOUT_MS. */
#define WIFI_CONNECT_TIMEOUT_MS         (  8 * 1000 )   /* normal: brief, then continue */
#define WIFI_TRIAL_CONNECT_TIMEOUT_MS   ( 45 * 1000 )   /* trial: block, else roll back */
#define CLOUD_TRIAL_CONNECT_TIMEOUT_MS  ( 60 * 1000 )   /* trial: cloud connect budget */

/* Publish outbox bound (both transports). Backpressure surfaces uniformly as a
 * bounded queue: transport_publish enqueues and returns; once the outbox holds
 * this many bytes / messages, further publishes return ESP_ERR_NO_MEM instead of
 * blocking (coreMQTT) or growing unbounded (esp-mqtt). QoS1 messages stay in the
 * outbox until PUBACK'd and are retransmitted. */
#define MQTT_OUTBOX_LIMIT_BYTES         ( 32 * 1024 )
#define MQTT_OUTBOX_MAX_MSGS            16
#define MQTT_QOS1_RETRANSMIT_MS         ( 10 * 1000 )

/* Self-test window. Covers Wi-Fi + the (bounded) initial MQTT connect + the
 * core-function check. If the trial image HANGS within this budget the Task WDT
 * panics -> reboot -> the uncommitted image is rolled back. (The bootloader RTC
 * WDT only guards pre-app_main boot; IDF disables it during system init when
 * BOOTLOADER_WDT_DISABLE_IN_USER_CODE=n, so this app-armed WDT is the self-test
 * guard.) Explicit failures (no Wi-Fi, no cloud, bad core check) reject and roll
 * back sooner than this. */
#define SELF_TEST_WATCHDOG_TIMEOUT_MS   ( 180 * 1000 )

/* NVS namespace/key used to carry the in-flight OTA job id across the reboot,
 * so the trial image can report SUCCEEDED/FAILED for it. */
#define OTA_NVS_NAMESPACE               "ota"
#define OTA_NVS_KEY_JOB_ID              "job_id"

/* Wrap a log-message string literal in bold-green / bold-red ANSI so the milestone
 * lines (signature OK, download failed, ...) stand out on the console. Literal-only
 * (concatenated at compile time), so use as ESP_LOGx(TAG, LOG_GREEN("..."), args). */
#define LOG_GREEN(str)  "\033[1;32m" str "\033[0m"
#define LOG_RED(str)    "\033[1;31m" str "\033[0m"

#endif /* APP_CONFIG_H */

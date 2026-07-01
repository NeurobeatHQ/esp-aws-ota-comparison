/*
 * app_main.c — the example application.
 *
 * This file uses ONLY device_iot.h and is byte-for-byte identical across all
 * four OTA backends. Pick a backend at build time:
 *     pio run -e mqtt | https | jobs | manual
 *
 * The app is unaware of coreMQTT vs esp-mqtt, AWS Jobs vs a custom protocol, or
 * MQTT-streams vs HTTPS download — that is the point of the device_iot facade.
 */
#include "device_iot.h"
#include "app_config.h"
#include "led.h"

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "app";

/* The application's post-OTA health gate (registered with device_iot). A real
 * app checks its critical services here; the demo honours the vGOOD/vBAD flag. */
static bool app_health_check(void)
{
    return FW_SELFTEST_SHOULD_PASS;
}

/* Example app command handler (subscribed below). cb receives the full topic. */
static void on_command(const char *topic, const uint8_t *data, size_t len)
{
    ESP_LOGI(TAG, "command on %s: %.*s", topic, (int)len, (const char *)data);
}

/* Connection hook: fires on first connect AND every reconnect (subscriptions are
 * already replayed by the facade), and on disconnect. Publish a retained-style
 * "online" birth message at QoS1 so it survives backpressure. */
static void on_connection(bool connected)
{
    if (!connected) {
        ESP_LOGW(TAG, "offline");
        return;
    }
    char birth[96];
    int n = snprintf(birth, sizeof(birth), "{\"online\":true,\"backend\":\"%s\",\"fw\":\"%d.%d.%d\"}",
                     device_iot_backend_name(), APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);
    device_iot_publish("status", birth, (size_t)n, 1);

    /* Report the firmware version into the classic Device Shadow so the fleet index is
     * queryable by a NUMERIC version (swVersion). "last seen" is stamped separately by a
     * cloud IoT rule on presence events (AWS-sourced time), so nothing here needs a clock. */
    char shadow[176];
    int m = snprintf(shadow, sizeof(shadow),
                     "{\"state\":{\"reported\":{\"swVersion\":%d,\"swVersionStr\":\"%d.%d.%d\",\"backend\":\"%s\"}}}",
                     APP_VERSION_INT, APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD,
                     device_iot_backend_name());
    char shadow_topic[96];
    snprintf(shadow_topic, sizeof(shadow_topic), "$aws/things/%s/shadow/update", device_iot_thing_name());
    device_iot_publish_topic(shadow_topic, shadow, (size_t)m, 1);
}

void app_main(void)
{
    /* Blink the major version on the onboard LED from the very first instant — runs
     * independently of the network, so the board's version (and any OTA-upgrade or
     * rollback) is visible by eye even before/without a cloud connection. */
    led_start_version_blink(APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD);

    device_iot_set_health_check(app_health_check);
    device_iot_set_connection_cb(on_connection);   /* before init: birth on connect */

    device_iot_config_t cfg;
    /* Identity (device cert + key + Thing name via the cert CN) from the esp_secure_cert
     * partition (provision-secure-cert.sh). ESP_ERR_INVALID_STATE = provisioned but the
     * cert has no CN (no identity) — NOT fatal: device_iot_init runs offline on a normal
     * boot / rolls back on a trial boot. Any other error (no cert/key) is a real failure. */
    esp_err_t idres = device_iot_default_config(&cfg);
    if (idres != ESP_OK && idres != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(idres);
    }
    ESP_ERROR_CHECK(device_iot_init(&cfg));

    device_iot_subscribe("cmd", on_command);

    ESP_LOGI(TAG, "running on '%s' backend, v%d.%d.%d (%s)",
             device_iot_backend_name(), APP_VERSION_MAJOR, APP_VERSION_MINOR,
             APP_VERSION_BUILD, FW_SELFTEST_SHOULD_PASS ? "vGOOD" : "vBAD");

    for (;;) {
        if (device_iot_is_connected()) {
            char payload[128];
            int n = snprintf(payload, sizeof(payload),
                             "{\"fw\":\"%d.%d.%d\",\"backend\":\"%s\",\"up\":%lld}",
                             APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD,
                             device_iot_backend_name(), (long long)esp_log_timestamp());
            device_iot_publish("heartbeat", payload, (size_t)n, 1);   /* QoS1 */
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

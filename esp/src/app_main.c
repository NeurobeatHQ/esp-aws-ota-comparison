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
}

void app_main(void)
{
    device_iot_set_health_check(app_health_check);
    device_iot_set_connection_cb(on_connection);   /* before init: birth on connect */

    device_iot_config_t cfg;
    device_iot_default_config(&cfg);          /* build-time identity (secrets.h + embedded certs) */
    ESP_ERROR_CHECK(device_iot_init(&cfg));   /* override cfg fields here to provision per device */

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

/*
 * app_main.c — variant B (jobs-esp). ESP32-S3 · AWS IoT Jobs (esp-mqtt) +
 * esp_https_ota self-download. Same boot/commit/rollback sequence as variant C;
 * only the MQTT engine (esp-mqtt) and the OTA data path (HTTPS) differ.
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"

#include "app_config.h"
#include "wifi.h"
#include "mqtt_es.h"
#include "self_test.h"
#include "jobs_orchestrator.h"

static const char *TAG = "app";

static void heartbeat_loop(void)
{
    char topic[160];
    int topic_len = snprintf(topic, sizeof(topic), "dt/%s/heartbeat", THING_NAME);
    for (;;) {
        if (mqtt_es_is_connected()) {
            char payload[96];
            int n = snprintf(payload, sizeof(payload),
                             "{\"fw\":\"%d.%d.%d\",\"variant\":\"%s\",\"up\":%lld}",
                             APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD,
                             FW_SELFTEST_SHOULD_PASS ? "vGOOD" : "vBAD",
                             (long long)esp_log_timestamp());
            mqtt_es_publish(topic, (uint16_t)topic_len, payload, (size_t)n, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void app_main(void)
{
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    self_test_log_boot_info();

    bool trial = self_test_is_trial_boot();
    if (trial) {
        self_test_arm_watchdog();
    }

    if (wifi_connect_blocking() != ESP_OK) {
        if (trial) {
            ESP_LOGE(TAG, "no Wi-Fi on trial image -> rolling back");
            self_test_reject_and_rollback();
        }
        ESP_LOGE(TAG, "no Wi-Fi; restarting in 5s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    if (trial) self_test_feed_watchdog();

    if (mqtt_es_start(jobs_orchestrator_on_publish) != ESP_OK) {
        ESP_LOGW(TAG, "initial MQTT connect did not complete (esp-mqtt keeps retrying)");
    }
    if (trial) self_test_feed_watchdog();

    jobs_orchestrator_start();   /* resolves trial boot + services OTA jobs */

    ESP_LOGI(TAG, "up on v%d.%d.%d (%s) — variant B (Jobs + esp_https_ota)",
             APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD,
             FW_SELFTEST_SHOULD_PASS ? "vGOOD" : "vBAD");

    heartbeat_loop();
}

/*
 * app_main.c — ESP32-S3 · AWS IoT Core · Modular OTA (rollback-safe) POC.
 *
 * Boot sequence:
 *   1. NVS + netif + default event loop
 *   2. if PENDING_VERIFY (trial boot): ARM the self-test watchdog up front so a
 *      hang anywhere in bring-up rolls the image back
 *   3. Wi-Fi  ->  MQTT(AWS IoT Core, mutual TLS)
 *   4. ota_orchestrator_start(): resolves the trial boot (self-test -> commit or
 *      reject/rollback), then services OTA jobs
 *   5. publish a heartbeat so the committed version is visible in the cloud
 *
 * pure ESP-IDF: app_main() entry, FreeRTOS tasks, ESP_LOGx — no Arduino.
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
#include "mqtt_client.h"
#include "self_test.h"
#include "ota_orchestrator.h"

static const char *TAG = "app";

static void heartbeat_loop(void)
{
    char topic[160];
    int topic_len = snprintf(topic, sizeof(topic), "dt/%s/heartbeat", THING_NAME);

    for (;;) {
        if (mqtt_client_is_connected()) {
            char payload[96];
            int n = snprintf(payload, sizeof(payload),
                             "{\"fw\":\"%d.%d.%d\",\"variant\":\"%s\",\"up\":%lld}",
                             APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD,
                             FW_SELFTEST_SHOULD_PASS ? "vGOOD" : "vBAD",
                             (long long)(esp_log_timestamp()));
            mqtt_client_publish(topic, (uint16_t)topic_len, payload, (size_t)n, 0);
        }
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

void app_main(void)
{
    /* --- base init --- */
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    self_test_log_boot_info();

    /* --- arm the self-test watchdog BEFORE the long bring-up on a trial boot --- */
    bool trial = self_test_is_trial_boot();
    if (trial) {
        self_test_arm_watchdog();
    }

    /* --- Wi-Fi --- */
    if (wifi_connect_blocking() != ESP_OK) {
        if (trial) {
            ESP_LOGE(TAG, "no Wi-Fi on trial image -> rolling back");
            self_test_reject_and_rollback();   /* does not return */
        }
        ESP_LOGE(TAG, "no Wi-Fi; restarting in 5s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }
    if (trial) self_test_feed_watchdog();

    /* --- MQTT to AWS IoT Core (mutual TLS) --- */
    if (mqtt_client_start(ota_orchestrator_on_publish) != ESP_OK) {
        ESP_LOGW(TAG, "initial MQTT connect did not complete (will keep retrying)");
        /* On a trial boot, ota_orchestrator_start() sees !connected -> self-test
         * fails -> rollback. On a normal boot, the background task reconnects. */
    }
    if (trial) self_test_feed_watchdog();

    /* --- resolve trial boot (commit/rollback) + start servicing OTA jobs --- */
    ota_orchestrator_start();

    ESP_LOGI(TAG, "up and running on v%d.%d.%d (%s)",
             APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD,
             FW_SELFTEST_SHOULD_PASS ? "vGOOD" : "vBAD");

    heartbeat_loop();
}

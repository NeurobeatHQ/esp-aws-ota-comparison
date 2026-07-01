/* self_test.c — trial-boot detection, self-test watchdog, commit/rollback, NVS. */
#include "self_test.h"
#include "app_config.h"
#include "transport.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "self_test";
static bool s_wdt_armed = false;

bool self_test_is_trial_boot(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;

    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        return state == ESP_OTA_IMG_PENDING_VERIFY;
    }
    /* No otadata entry (e.g. the very first esptool flash to ota_0) -> not a
     * trial image; run normally. */
    return false;
}

void self_test_log_boot_info(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    (void)esp_ota_get_state_partition(running, &state);

    ESP_LOGI(TAG, "----------------------------------------------------------");
    ESP_LOGI(TAG, " firmware v%d.%d.%d   variant=%s",
             APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_BUILD,
             FW_SELFTEST_SHOULD_PASS ? "vGOOD" : "vBAD");
    ESP_LOGI(TAG, " running partition: %s @ 0x%06" PRIx32 "  (ota state %d)",
             running->label, running->address, (int)state);
    ESP_LOGI(TAG, " reset reason: %d   free heap: %u",
             (int)esp_reset_reason(), (unsigned)esp_get_free_heap_size());
    ESP_LOGI(TAG, "----------------------------------------------------------");
}

void self_test_arm_watchdog(void)
{
    esp_task_wdt_config_t cfg = {
        .timeout_ms = SELF_TEST_WATCHDOG_TIMEOUT_MS,
        .idle_core_mask = 0,
        .trigger_panic = true,   /* timeout -> panic -> reboot -> rollback */
    };
    /* ESP_TASK_WDT_INIT=n, so this is the first init. */
    esp_err_t err = esp_task_wdt_init(&cfg);
    if (err == ESP_ERR_INVALID_STATE) {
        err = esp_task_wdt_reconfigure(&cfg);   /* already running somehow */
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    s_wdt_armed = true;
    ESP_LOGW(TAG, "self-test watchdog armed (%d ms)", SELF_TEST_WATCHDOG_TIMEOUT_MS);
}

void self_test_feed_watchdog(void)
{
    if (s_wdt_armed) {
        esp_task_wdt_reset();
    }
}

void self_test_disarm_watchdog(void)
{
    if (s_wdt_armed) {
        esp_task_wdt_delete(NULL);
        esp_task_wdt_deinit();
        s_wdt_armed = false;
        ESP_LOGI(TAG, "self-test watchdog disarmed");
    }
}

/* Optional application-provided health check (device_iot_set_health_check). */
static bool (*s_app_health_cb)(void);

void self_test_set_health_cb(bool (*cb)(void))
{
    s_app_health_cb = cb;
}

bool self_test_core_function_ok(void)
{
    /* Always do a token platform check (heap), then defer to the application's
     * health check if one was registered; otherwise honour the build-time
     * variant flag so the vBAD fixture deterministically fails this gate. */
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    ESP_LOGI(TAG, "core-function check: free heap = %u", (unsigned)free_heap);

    if (free_heap < 20 * 1024) {
        ESP_LOGE(TAG, "core-function check FAILED: heap exhausted");
        return false;
    }

    if (s_app_health_cb != NULL) {
        bool ok = s_app_health_cb();
        ESP_LOGI(TAG, "app health check -> %s", ok ? "PASS" : "FAIL");
        return ok;
    }

#if FW_SELFTEST_SHOULD_PASS
    ESP_LOGI(TAG, "core-function check PASSED (vGOOD)");
    return true;
#else
    ESP_LOGE(TAG, "core-function check FAILED (vBAD: FW_SELFTEST_SHOULD_PASS=0)");
    return false;
#endif
}

void self_test_commit(void)
{
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "\033[1;32mimage COMMITTED (rollback cancelled)\033[0m");
    } else {
        ESP_LOGE(TAG, "esp_ota_mark_app_valid_cancel_rollback failed: %s",
                 esp_err_to_name(err));
    }
}

void self_test_reject_and_rollback(void)
{
    ESP_LOGE(TAG, "\033[1;31mimage REJECTED -> rolling back to previous slot\033[0m");
    /* Writes ESP_OTA_IMG_INVALID to otadata and reboots; the bootloader then
     * boots the previously-valid partition. Does not return. */
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    /* Only reached if there is no valid image to roll back to. */
    ESP_LOGE(TAG, "rollback failed (%s); restarting anyway", esp_err_to_name(err));
    esp_restart();
}

void self_test_resolve_trial(void (*report)(const char *jobId, bool ok),
                             uint32_t commit_delay_ms)
{
    char jobId[64] = { 0 };
    bool haveJob = (ota_nvs_get_job_id(jobId, sizeof(jobId)) == ESP_OK);
    ESP_LOGW(TAG, "trial boot detected — running self-test%s", haveJob ? "" : " (no stashed job id)");

    bool cloud_ok = transport_is_connected();
    bool core_ok  = self_test_core_function_ok();

    if (cloud_ok && core_ok) {
        self_test_commit();
        self_test_disarm_watchdog();
        if (haveJob) {
            report(jobId, true);
        }
        /* Give the cloud a moment to record the report before StartNext. */
        vTaskDelay(pdMS_TO_TICKS(commit_delay_ms));
    } else {
        ESP_LOGE(TAG, "self-test failed (cloud=%d core=%d) -> rollback", cloud_ok, core_ok);
        if (haveJob) {
            report(jobId, false);
        }
        self_test_reject_and_rollback();   /* does not return */
    }
}

/* ---------------------------- NVS job-id hand-off ------------------------- */

esp_err_t ota_nvs_set_job_id(const char *job_id)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(h, OTA_NVS_KEY_JOB_ID, job_id);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t ota_nvs_get_job_id(char *out, size_t out_len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_str(h, OTA_NVS_KEY_JOB_ID, out, &out_len);
    nvs_close(h);
    return err;
}

void ota_nvs_clear_job_id(void)
{
    nvs_handle_t h;
    if (nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, OTA_NVS_KEY_JOB_ID);
        nvs_commit(h);
        nvs_close(h);
    }
}

/* ota_download.c — esp_https_ota self-download helper (variants A + B). */
#include "ota_download.h"
#include "app_config.h"   /* LOG_GREEN */

#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_log.h"

static const char *TAG = "ota_dl";

/* S3 presigned URLs chain to Amazon Root CA 1 — the same anchor used for IoT. */
extern const unsigned char aws_root_ca_pem[];

esp_err_t ota_download_run(const char *url)
{
    esp_http_client_config_t http_cfg = {
        .url = url,
        .cert_pem = (const char *)aws_root_ca_pem,
        .keep_alive_enable = true,
        .timeout_ms = 20000,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    ESP_LOGI(TAG, "starting HTTPS OTA download");
    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    int image_size = esp_https_ota_get_image_size(handle);
    int last_logged = 0;
    while ((err = esp_https_ota_perform(handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        int read = esp_https_ota_get_image_len_read(handle);
        if (image_size > 0 && read - last_logged >= image_size / 10) {
            ESP_LOGI(TAG, "downloaded %d / %d bytes (%d%%)", read, image_size,
                     read * 100 / image_size);
            last_logged = read;
        }
    }

    if (err != ESP_OK || !esp_https_ota_is_complete_data_received(handle)) {
        ESP_LOGE(TAG, "download failed/incomplete: %s", esp_err_to_name(err));
        esp_https_ota_abort(handle);
        return (err == ESP_OK) ? ESP_FAIL : err;
    }

    err = esp_https_ota_finish(handle);   /* validates + esp_ota_set_boot_partition */
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, LOG_GREEN("image downloaded + selected for next boot"));
    return ESP_OK;
}

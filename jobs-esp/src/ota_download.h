/*
 * ota_download.h — firmware self-download via esp_https_ota (variants A + B).
 *
 * Downloads from a presigned HTTPS (S3) URL and sets the new image as the next
 * boot partition. esp_https_ota does esp_ota_begin/write/end/set_boot_partition
 * internally — no manual PAL, no on-device code-signature check (TLS-to-S3 +
 * optional Secure Boot is the authenticity). On success the CALLER persists the
 * job/ota id to NVS and then calls esp_restart().
 */
#ifndef OTA_DOWNLOAD_H
#define OTA_DOWNLOAD_H

#include "esp_err.h"

/* Returns ESP_OK once the image is downloaded, validated and selected for boot. */
esp_err_t ota_download_run(const char *url);

#endif /* OTA_DOWNLOAD_H */

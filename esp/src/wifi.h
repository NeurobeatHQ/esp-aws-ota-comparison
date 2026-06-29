/* wifi.h — Wi-Fi station bring-up with indefinite background reconnect. */
#ifndef WIFI_H
#define WIFI_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Initialise + start Wi-Fi station. NON-BLOCKING: begins connecting and keeps
 * (re)connecting in the background indefinitely (so the device comes online when
 * the AP appears, and recovers from drops). Call once after nvs/netif/event init. */
esp_err_t wifi_start(void);

/* Block up to timeout_ms for an IP. ESP_OK if connected, ESP_ERR_TIMEOUT if the
 * budget expires — Wi-Fi keeps retrying in the background either way. */
esp_err_t wifi_wait_connected(uint32_t timeout_ms);

/* Currently associated with an IP? */
bool wifi_is_connected(void);

#endif /* WIFI_H */

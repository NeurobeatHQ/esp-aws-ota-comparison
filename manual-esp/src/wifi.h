/* wifi.h — minimal Wi-Fi station bring-up. */
#ifndef WIFI_H
#define WIFI_H

#include "esp_err.h"

/* Initialise Wi-Fi in station mode and block until an IP is acquired (or all
 * retries are exhausted). Safe to call once after nvs/netif/event-loop init. */
esp_err_t wifi_connect_blocking(void);

#endif /* WIFI_H */

// nbt factory / recovery image — pure ESP-IDF, single file.
//
// Entered by the bootloader when no OTA slot is bootable, or via the GPIO
// factory-reset. Behavior (design doc §4): always bring up an open SoftAP
// `nbt-factory-ap` with a captive portal (SSID, password, URL). On submit it
// joins the operator's WiFi and HTTP-OTAs the signed image at the URL into an
// OTA slot, then reboots into it.
//
// Authenticity of the downloaded image is the Secure Boot signature, so the OTA
// runs over plain HTTP with no trust store. esp_https_ota writes to the next OTA
// slot (never `factory`), so the recovery flow can't brick itself.

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "factory";

#define FACTORY_AP_SSID "nbt-factory-ap"
#define BIT_GOT_IP BIT0
#define BIT_FAIL   BIT1
#define MAX_RETRY  8

// ---------------------------------------------------------------- WiFi (AP+STA)

static EventGroupHandle_t s_eg;
static int s_retries;

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retries++ < MAX_RETRY) {
            esp_wifi_connect();             // keep trying until GOT_IP or MAX_RETRY
        } else if (s_eg) {
            xEventGroupSetBits(s_eg, BIT_FAIL);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retries = 0;
        if (s_eg) xEventGroupSetBits(s_eg, BIT_GOT_IP);
    }
}

// Open SoftAP (192.168.4.1) in AP+STA mode, so the portal stays reachable while
// we also connect out to the operator's network to download.
static void net_apsta_start(const char *ap_ssid)
{
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t c = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&c));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL, NULL));

    wifi_config_t ap = {0};
    strlcpy((char *)ap.ap.ssid, ap_ssid, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(ap_ssid);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "SoftAP '%s' up at 192.168.4.1 (AP+STA)", ap_ssid);
}

// Join the operator's network as a station (the AP stays up). true on GOT_IP.
static bool net_sta_join(const char *ssid, const char *pass, int timeout_ms)
{
    if (!s_eg) s_eg = xEventGroupCreate();
    xEventGroupClearBits(s_eg, BIT_GOT_IP | BIT_FAIL);
    s_retries = 0;

    wifi_config_t sta = {0};
    strlcpy((char *)sta.sta.ssid, ssid, sizeof(sta.sta.ssid));
    strlcpy((char *)sta.sta.password, pass ? pass : "", sizeof(sta.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta));
    esp_wifi_connect();

    EventBits_t b = xEventGroupWaitBits(s_eg, BIT_GOT_IP | BIT_FAIL,
                                        pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));
    bool ok = (b & BIT_GOT_IP) != 0;
    ESP_LOGI(TAG, "STA join '%s': %s", ssid, ok ? "ok" : "failed");
    return ok;
}

// ------------------------------------------------------------------------- OTA

// Pull the image at a plain-HTTP `url` into the next OTA slot using a raw lwIP
// socket + esp_ota_*, then reboot into it. No esp_http_client / esp-tls / mbedTLS:
// authenticity is the Secure Boot signature — verified before the slot is made
// bootable (when SB is on) and re-verified by the bootloader at boot. Assumes a
// simple static HTTP server (HTTP/1.0, no chunked transfer-encoding).
static bool recovery_ota(const char *url)
{
    ESP_LOGI(TAG, "recovery OTA from %s", url);

    // parse "http://host[:port]/path"
    if (strncmp(url, "http://", 7) != 0) { ESP_LOGE(TAG, "url must be http://"); return false; }
    const char *hp = url + 7;
    const char *slash = strchr(hp, '/');
    char host[128], port[8] = "80", path[256];
    size_t hlen = slash ? (size_t)(slash - hp) : strlen(hp);
    if (hlen == 0 || hlen >= sizeof(host)) return false;
    memcpy(host, hp, hlen); host[hlen] = '\0';
    strlcpy(path, slash ? slash : "/", sizeof(path));
    char *colon = strchr(host, ':');
    if (colon) { *colon = '\0'; strlcpy(port, colon + 1, sizeof(port)); }

    // resolve + connect
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, port, &hints, &res) != 0 || !res) { ESP_LOGE(TAG, "DNS '%s'", host); return false; }
    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    bool connected = (sock >= 0) && (connect(sock, res->ai_addr, res->ai_addrlen) == 0);
    freeaddrinfo(res);
    if (!connected) { if (sock >= 0) close(sock); ESP_LOGE(TAG, "connect %s:%s", host, port); return false; }

    // minimal HTTP/1.0 GET; "Connection: close" -> server closes at body end (no chunked)
    char req[420];
    int rl = snprintf(req, sizeof(req),
        "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: nbt-factory\r\nConnection: close\r\n\r\n", path, host);
    if (send(sock, req, rl, 0) != rl) { close(sock); return false; }

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL); // an OTA slot, never factory
    esp_ota_handle_t ota = 0;
    if (!part || esp_ota_begin(part, OTA_SIZE_UNKNOWN, &ota) != ESP_OK) { close(sock); return false; }

    // stream: skip headers (sliding window for CRLFCRLF), require "200" status, write body
    char buf[1024];
    uint8_t w4[4] = {0};
    char line0[48]; int l0 = 0; bool eol0 = false;
    bool in_body = false, wrote = false, fail = false;
    int n;
    while ((n = recv(sock, buf, sizeof(buf), 0)) > 0) {
        int i = 0;
        if (!in_body) {
            for (; i < n; i++) {
                char c = buf[i];
                if (!eol0) { if (c == '\n') eol0 = true; else if (c != '\r' && l0 < (int)sizeof(line0) - 1) line0[l0++] = c; }
                w4[0] = w4[1]; w4[1] = w4[2]; w4[2] = w4[3]; w4[3] = (uint8_t)c;
                if (w4[0] == '\r' && w4[1] == '\n' && w4[2] == '\r' && w4[3] == '\n') { in_body = true; i++; break; }
            }
            if (in_body) {
                line0[l0] = '\0';
                if (!strstr(line0, " 200")) { ESP_LOGE(TAG, "HTTP status: %s", line0); fail = true; break; }
            }
        }
        if (in_body && i < n) {
            if (esp_ota_write(ota, buf + i, n - i) != ESP_OK) { fail = true; break; }
            wrote = true;
        }
    }
    close(sock);

    if (fail || n < 0 || !in_body || !wrote) { esp_ota_abort(ota); ESP_LOGE(TAG, "download failed"); return false; }
    if (esp_ota_end(ota) != ESP_OK || esp_ota_set_boot_partition(part) != ESP_OK) {
        ESP_LOGE(TAG, "image invalid / not bootable");
        return false;
    }
    ESP_LOGI(TAG, "OTA ok — rebooting into the recovered image");
    esp_restart();
    return true; // not reached
}

// Background worker: join WiFi + OTA. recovery_ota() reboots on success; on
// failure we leave the portal up so the operator can retry.
typedef struct { char ssid[33]; char pass[65]; char url[256]; } recover_args_t;

static void recover_task(void *arg)
{
    recover_args_t *a = (recover_args_t *) arg;
    if (net_sta_join(a->ssid, a->pass, 30000)) {
        recovery_ota(a->url);  // reboots on success
        ESP_LOGW(TAG, "OTA failed; portal still up for retry");
    } else {
        ESP_LOGW(TAG, "could not join '%s'; portal still up for retry", a->ssid);
    }
    free(a);
    vTaskDelete(NULL);
}

// -------------------------------------------------------------- Captive portal

static const char FORM[] =
    "<!doctype html><html><head>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>nbt factory recovery</title></head>"
    "<body style='font-family:sans-serif;max-width:420px;margin:2em auto'>"
    "<h2>Factory recovery</h2>"
    "<form method=post action=/save>"
    "<p>WiFi SSID<br><input name=ssid style='width:100%' required></p>"
    "<p>WiFi password<br><input name=pass type=password style='width:100%'></p>"
    "<p>Recovery URL (signed image)<br><input name=url style='width:100%' "
    "placeholder='http://host/recovery.bin' required></p>"
    "<p><button type=submit>Connect &amp; recover</button></p>"
    "</form></body></html>";

// minimal application/x-www-form-urlencoded field extractor
static void form_field(const char *body, const char *key, char *out, size_t len)
{
    out[0] = '\0';
    char pat[40];
    snprintf(pat, sizeof(pat), "%s=", key);
    const char *p = strstr(body, pat);
    if (!p) return;
    p += strlen(pat);
    size_t i = 0;
    while (*p && *p != '&' && i + 1 < len) {
        char c = *p++;
        if (c == '+') {
            c = ' ';
        } else if (c == '%' && p[0] && p[1]) {
            char h[3] = { p[0], p[1], '\0' };
            c = (char) strtol(h, NULL, 16);
            p += 2;
        }
        out[i++] = c;
    }
    out[i] = '\0';
}

static esp_err_t get_form(httpd_req_t *r)
{
    httpd_resp_set_type(r, "text/html");
    return httpd_resp_send(r, FORM, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t post_save(httpd_req_t *r)
{
    char body[640];
    int n = httpd_req_recv(r, body, sizeof(body) - 1);
    if (n <= 0) return ESP_FAIL;
    body[n] = '\0';

    recover_args_t *a = calloc(1, sizeof(*a));
    if (!a) return ESP_FAIL;
    form_field(body, "ssid", a->ssid, sizeof(a->ssid));
    form_field(body, "pass", a->pass, sizeof(a->pass));
    form_field(body, "url",  a->url,  sizeof(a->url));

    // Respond BEFORE touching WiFi: joining the STA moves the SoftAP to the
    // operator's channel, which can briefly drop the phone off this AP.
    httpd_resp_set_type(r, "text/html");
    httpd_resp_sendstr(r, "<html><body style='font-family:sans-serif'>"
                          "<h3>Connecting and recovering&hellip;</h3>"
                          "<p>The device reboots into the new image if it succeeds. "
                          "If it stays on this AP, recovery failed &mdash; retry.</p></body></html>");

    xTaskCreate(recover_task, "recover", 8192, a, 5, NULL);
    return ESP_OK;
}

// catch-all GET so OS captive-portal probes get redirected to the form
static esp_err_t get_catchall(httpd_req_t *r)
{
    httpd_resp_set_status(r, "302 Found");
    httpd_resp_set_hdr(r, "Location", "http://192.168.4.1/");
    return httpd_resp_send(r, NULL, 0);
}

static void start_http(void)
{
    httpd_handle_t s = NULL;
    httpd_config_t c = HTTPD_DEFAULT_CONFIG();
    c.uri_match_fn = httpd_uri_match_wildcard;
    c.lru_purge_enable = true;
    ESP_ERROR_CHECK(httpd_start(&s, &c));

    httpd_uri_t u_form = { .uri = "/",     .method = HTTP_GET,  .handler = get_form };
    httpd_uri_t u_save = { .uri = "/save", .method = HTTP_POST, .handler = post_save };
    httpd_uri_t u_any  = { .uri = "/*",    .method = HTTP_GET,  .handler = get_catchall };
    httpd_register_uri_handler(s, &u_form);
    httpd_register_uri_handler(s, &u_save);
    httpd_register_uri_handler(s, &u_any);
}

// tiny DNS server: answer every single-question A query with 192.168.4.1
static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { vTaskDelete(NULL); return; }
    struct sockaddr_in sa = {
        .sin_family = AF_INET,
        .sin_port = htons(53),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    const uint8_t answer[] = { 0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 192, 168, 4, 1 };
    uint8_t buf[512];
    while (1) {
        struct sockaddr_in src;
        socklen_t sl = sizeof(src);
        int n = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr *)&src, &sl);
        if (n < 12 || n + (int)sizeof(answer) > (int)sizeof(buf)) continue;
        buf[2] |= 0x80;  // QR = response
        buf[7] = 1;      // ANCOUNT = 1 (assumes the one question we echo back)
        memcpy(buf + n, answer, sizeof(answer));
        sendto(sock, buf, n + sizeof(answer), 0, (struct sockaddr *)&src, sl);
    }
}

static void portal_start(void)
{
    start_http();
    xTaskCreate(dns_task, "dns", 3072, NULL, 5, NULL);
    ESP_LOGI(TAG, "captive portal up — join '%s' and open http://192.168.4.1", FACTORY_AP_SSID);
}

// ------------------------------------------------------------------------ main

void app_main(void)
{
    esp_err_t err = nvs_flash_init();   // esp_wifi needs NVS for PHY/calibration data
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_LOGI(TAG, "factory recovery: bringing up portal '%s'", FACTORY_AP_SSID);
    net_apsta_start(FACTORY_AP_SSID);
    portal_start();   // serves the 3-field form; on submit -> join WiFi + OTA the URL

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

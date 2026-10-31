#include "net.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "place.h"
#include "rtc.h"

#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif
#ifndef TOMO_WIFI_SSID
#define TOMO_WIFI_SSID ""
#define TOMO_WIFI_PASSWORD ""
#endif
#ifndef TOMO_HOME_SSID
#define TOMO_HOME_SSID TOMO_WIFI_SSID
#endif

static const char *TAG = "net";

// Europe/London: GMT, or BST from the last Sunday in March (01:00) to the
// last Sunday in October (02:00).
#define LONDON_TZ "GMT0BST,M3.5.0/1,M10.5.0"
// Anything before this is a clock that was never set.
#define SANE_UTC 1767225600  // 2026-01-01

#define SCAN_EVERY_MS (5u * 60u * 1000u)  // Niall: no more often than this
#define FIRST_SCAN_MS 10000u              // let the first connect finish
#define MAX_APS 24

static bool s_have_creds;
static volatile bool s_connected;
static place_t s_place;
static portMUX_TYPE s_place_lock = portMUX_INITIALIZER_UNLOCKED;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// --- Time ----------------------------------------------------------------------

static void restore_time_from_rtc(void)
{
    if (rtc_start() != ESP_OK) {
        return;
    }
    time_t utc;
    esp_err_t err = rtc_read(&utc);
    if (err != ESP_OK || utc < SANE_UTC) {
        ESP_LOGW(TAG, "RTC has no good time (%s); waiting for SNTP", esp_err_to_name(err));
        return;
    }
    struct timeval tv = {.tv_sec = utc};
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "time from RTC: %lld", (long long)utc);
}

static void on_time_sync(struct timeval *tv)
{
    ESP_LOGI(TAG, "time from SNTP: %lld", (long long)tv->tv_sec);
    if (tv->tv_sec >= SANE_UTC && rtc_write(tv->tv_sec) != ESP_OK) {
        ESP_LOGW(TAG, "couldn't save time to RTC");
    }
}

// --- Wi-Fi ---------------------------------------------------------------------

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_have_creds) {
            esp_wifi_connect();
        }
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        // Don't retry here: the scan task reconnects after each scan, so
        // connect attempts never get in a scan's way.
        s_connected = false;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_connected = true;
        ESP_LOGI(TAG, "connected to %s", TOMO_WIFI_SSID);
    }
}

static void scan_once(void)
{
    static wifi_ap_record_t aps[MAX_APS];  // ~2.5 KB; keep it off the stack
    esp_err_t err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan failed: %s", esp_err_to_name(err));
        return;
    }
    uint16_t n = MAX_APS;
    err = esp_wifi_scan_get_ap_records(&n, aps);
    esp_wifi_clear_ap_list();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "scan results failed: %s", esp_err_to_name(err));
        return;
    }

    const char *ssids[MAX_APS];
    for (uint16_t i = 0; i < n; i++) {
        ssids[i] = (const char *)aps[i].ssid;
        // Logged so a trip on the tube shows which networks are really there.
        ESP_LOGI(TAG, "seen: \"%s\" %d dBm%s", ssids[i], aps[i].rssi,
                 place_is_tfl_ssid(ssids[i]) ? " (tube)" : "");
    }

    uint32_t now = now_ms();
    taskENTER_CRITICAL(&s_place_lock);
    place_scan(&s_place, ssids, n, now);
    pet_place_t place = place_now(&s_place, now);
    taskEXIT_CRITICAL(&s_place_lock);
    ESP_LOGI(TAG, "%u networks, place=%d", n, place);
}

static void scan_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(FIRST_SCAN_MS));
    for (;;) {
        scan_once();
        if (s_have_creds && !s_connected) {
            esp_wifi_connect();
        }
        vTaskDelay(pdMS_TO_TICKS(SCAN_EVERY_MS));
    }
}

static esp_err_t start_wifi(void)
{
    esp_err_t err = nvs_flash_init();  // the Wi-Fi driver keeps calibration here
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;
    if ((err = esp_netif_init()) != ESP_OK) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if ((err = esp_wifi_init(&init)) != ESP_OK) return err;
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL);
    if ((err = esp_wifi_set_mode(WIFI_MODE_STA)) != ESP_OK) return err;

    if (s_have_creds) {
        wifi_config_t cfg = {0};
        strncpy((char *)cfg.sta.ssid, TOMO_WIFI_SSID, sizeof cfg.sta.ssid);
        strncpy((char *)cfg.sta.password, TOMO_WIFI_PASSWORD, sizeof cfg.sta.password);
        if ((err = esp_wifi_set_config(WIFI_IF_STA, &cfg)) != ESP_OK) return err;

        esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        sntp.sync_cb = on_time_sync;
        if ((err = esp_netif_sntp_init(&sntp)) != ESP_OK) return err;
    } else {
        ESP_LOGW(TAG, "no wifi_secrets.h: no clock sync, scanning only");
    }
    return esp_wifi_start();
}

// --- Public ----------------------------------------------------------------------

esp_err_t net_start(void)
{
    setenv("TZ", LONDON_TZ, 1);
    tzset();
    restore_time_from_rtc();

    s_have_creds = TOMO_WIFI_SSID[0] != '\0';
    place_init(&s_place, TOMO_HOME_SSID);

    esp_err_t err = start_wifi();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi didn't start: %s", esp_err_to_name(err));
        return err;
    }
    if (xTaskCreate(scan_task, "wifi_scan", 4096, NULL, 2, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

pet_place_t net_place(uint32_t now_ms)
{
    taskENTER_CRITICAL(&s_place_lock);
    pet_place_t place = place_now(&s_place, now_ms);
    taskEXIT_CRITICAL(&s_place_lock);
    return place;
}

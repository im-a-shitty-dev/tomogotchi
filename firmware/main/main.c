// tomogotchi firmware entry point.
//
// Inputs (touch, motion) post events to a queue. The main loop feeds them and
// the clock into the pet logic, and redraws when the pet says something changed.
#include <time.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "motion.h"
#include "pet.h"
#include "screen.h"

static const char *TAG = "tomogotchi";

#define TICK_MS 100

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static pet_clock_t local_clock(void)
{
    // TODO: set the clock (RTC + SNTP) with the Wi-Fi work. Until then this
    // starts at midnight, 1 Jan 1970 (a Thursday) on every boot.
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    return (pet_clock_t){
        .month = tm.tm_mon + 1,
        .mday = tm.tm_mday,
        .wday = tm.tm_wday,
        .hour = tm.tm_hour,
        .minute = tm.tm_min,
    };
}

void app_main(void)
{
    QueueHandle_t events = xQueueCreate(8, sizeof(pet_event_t));

    ESP_ERROR_CHECK(screen_start(events));
    if (motion_start(events) != ESP_OK) {
        ESP_LOGW(TAG, "running without motion sensing");
    }

    pet_t pet;
    pet_init(&pet, esp_random());
    pet_clock_t clock = local_clock();
    pet_tick(&pet, &clock, now_ms());
    screen_draw(&pet);
    ESP_LOGI(TAG, "hello, mood=%s", pet_mood_name(pet_view(&pet).mood));

    for (;;) {
        bool dirty = false;
        pet_event_t ev;
        if (xQueueReceive(events, &ev, pdMS_TO_TICKS(TICK_MS)) == pdTRUE) {
            dirty |= pet_handle_event(&pet, ev, now_ms());
        }
        clock = local_clock();
        dirty |= pet_tick(&pet, &clock, now_ms());
        if (dirty) {
            screen_draw(&pet);
        }
    }
}

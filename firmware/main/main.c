// tomogotchi firmware entry point.
//
// Inputs (touch, motion) post events to a queue. The main loop feeds them and
// the clock into the pet logic as they arrive, and draws a frame every
// SCENE_TICK_MS (about 9 fps) so Tomo can blink, bob and so on. The screen
// only sends the part of each frame that changed.
#include <time.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "motion.h"
#include "net.h"
#include "pet.h"
#include "screen.h"

static const char *TAG = "tomogotchi";

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static pet_clock_t local_clock(void)
{
    // London time, set by net_start() from the RTC and SNTP. Until either
    // has a good time this is midnight, 1 Jan 1970 (a Thursday).
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
    if (net_start() != ESP_OK) {
        ESP_LOGW(TAG, "running without Wi-Fi: schedule only");
    }

    pet_t pet;
    pet_init(&pet, esp_random());
    pet_set_place(&pet, net_place(now_ms()));
    pet_clock_t clock = local_clock();
    pet_tick(&pet, &clock, now_ms());
    ESP_LOGI(TAG, "hello, mood=%s", pet_mood_name(pet_view(&pet).mood));

    uint32_t tick = 0;
    TickType_t next_frame = xTaskGetTickCount();
    for (;;) {
        // Handle input until the next frame is due.
        TickType_t now = xTaskGetTickCount();
        TickType_t wait = (TickType_t)(next_frame - now) <= pdMS_TO_TICKS(SCENE_TICK_MS) ? next_frame - now : 0;
        pet_event_t ev;
        if (xQueueReceive(events, &ev, wait) == pdTRUE) {
            pet_handle_event(&pet, ev, now_ms());
            continue;
        }
        pet_set_place(&pet, net_place(now_ms()));
        clock = local_clock();
        pet_tick(&pet, &clock, now_ms());
        screen_draw(&pet, tick++);
        next_frame += pdMS_TO_TICKS(SCENE_TICK_MS);
        if ((TickType_t)(xTaskGetTickCount() - next_frame) < (TickType_t)0x80000000u) {
            next_frame = xTaskGetTickCount() + pdMS_TO_TICKS(SCENE_TICK_MS);  // fell behind: don't try to catch up
        }
    }
}

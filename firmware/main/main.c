// tomogotchi firmware entry point.
//
// Inputs (touch, motion) post events to a queue. The main loop feeds them and
// the clock into the pet logic, and redraws when the pet says something changed.
#include <time.h>

#include "esp_log.h"
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

static int local_hour(void)
{
    // TODO: set the clock from the on-board RTC (PCF85063). Until then this
    // starts at midnight on every boot.
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    return tm.tm_hour;
}

void app_main(void)
{
    QueueHandle_t events = xQueueCreate(8, sizeof(pet_event_t));

    ESP_ERROR_CHECK(screen_start(events));
    if (motion_start(events) != ESP_OK) {
        ESP_LOGW(TAG, "running without motion sensing");
    }

    pet_t pet;
    pet_init(&pet);
    pet_tick(&pet, local_hour(), now_ms());
    screen_draw(&pet);
    ESP_LOGI(TAG, "hello, mood=%s", pet_mood_name(pet.mood));

    for (;;) {
        bool dirty = false;
        pet_event_t ev;
        if (xQueueReceive(events, &ev, pdMS_TO_TICKS(TICK_MS)) == pdTRUE) {
            dirty |= pet_handle_event(&pet, ev, now_ms());
        }
        dirty |= pet_tick(&pet, local_hour(), now_ms());
        if (dirty) {
            screen_draw(&pet);
        }
    }
}

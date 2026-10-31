// tomogotchi firmware entry point.
//
// Inputs (touch, motion) post events to a queue. The main loop feeds them and
// the clock into the pet logic as they arrive, and draws a frame every
// SCENE_TICK_MS (about 9 fps) so Tomo can blink, bob and so on. The screen
// only sends the part of each frame that changed.
//
// To save battery the screen sleeps after 30 seconds without a touch (see
// components/wake). While it's dark we "doze": no drawing, no motion
// sensing, and the chip light-sleeps between pet ticks once a second. A
// button press or a tap brings him back. On the charger the screen stays on
// for an hour without a touch.
#include <time.h>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "motion.h"
#include "net.h"
#include "pet.h"
#include "power.h"
#include "screen.h"
#include "wake.h"

static const char *TAG = "tomogotchi";

#define DOZE_TICK_MS 1000  // how often he lives his life while the screen is dark

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

// Check the buttons and charger; switch the screen on or off to match.
// Returns whether the screen is on.
static bool update_power(wake_t *wake, pet_t *pet, bool *on)
{
    uint32_t now = now_ms();
    power_status_t st = power_poll();
    wake_set_charging(wake, st.charging, now);
    pet_set_charging(pet, st.charging);
    if (st.tap) {
        wake_tap(wake, now);
    }
    if (st.button) {
        wake_button(wake, now);
    }

    static int last_pct = -2;
    if (st.battery_pct != last_pct) {
        ESP_LOGI(TAG, "battery %d%%%s", st.battery_pct, st.charging ? " (charging)" : "");
        last_pct = st.battery_pct;
    }

    bool want = wake_update(wake, now);
    if (want != *on) {
        *on = want;
        if (want) {
            power_set_dozing(false);
            screen_set_on(true);
            motion_set_active(true);
        } else {
            motion_set_active(false);
            screen_set_on(false);
            power_set_dozing(true);
        }
    }
    return want;
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
    if (power_start(xTaskGetCurrentTaskHandle()) != ESP_OK) {
        ESP_LOGW(TAG, "running without power saving");
    }

    pet_t pet;
    pet_init(&pet, esp_random());
    pet_set_place(&pet, net_place(now_ms()));
    pet_clock_t clock = local_clock();
    pet_tick(&pet, &clock, now_ms());
    ESP_LOGI(TAG, "hello, mood=%s", pet_mood_name(pet_view(&pet).mood));

    wake_t wake;
    wake_init(&wake, now_ms());
    bool on = true;

    uint32_t tick = 0;
    TickType_t next_frame = xTaskGetTickCount();
    for (;;) {
        if (!update_power(&wake, &pet, &on)) {
            // Dark: tick him once a second, or sooner if a button or tap
            // wakes us. The chip light-sleeps in between.
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(DOZE_TICK_MS));
            pet_set_place(&pet, net_place(now_ms()));
            clock = local_clock();
            pet_tick(&pet, &clock, now_ms());
            xQueueReset(events);  // nothing to react to while he can't be seen
            next_frame = xTaskGetTickCount();
            continue;
        }

        // Handle input until the next frame is due.
        TickType_t now = xTaskGetTickCount();
        TickType_t wait = (TickType_t)(next_frame - now) <= pdMS_TO_TICKS(SCENE_TICK_MS) ? next_frame - now : 0;
        pet_event_t ev;
        if (xQueueReceive(events, &ev, wait) == pdTRUE) {
            pet_handle_event(&pet, ev, now_ms());
            wake_activity(&wake, now_ms());
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

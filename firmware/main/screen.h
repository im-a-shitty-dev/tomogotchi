// The pixel-art screen: a small logical canvas scaled up to the AMOLED panel.
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "pet.h"
#include "scene.h"

// Logical canvas, in "art pixels". 52x64 at 7x = 364x448 on the 368x448
// panel, leaving 2px black bars at each side.
#define SCREEN_ART_W SCENE_W
#define SCREEN_ART_H SCENE_H
#define SCREEN_ART_SCALE 7

// Start the display and touch. Taps are posted to `events` as PET_EVENT_POKE.
esp_err_t screen_start(QueueHandle_t events);

// Turn the panel off (asleep, LVGL stopped) or back on. While off,
// screen_draw() does nothing and touches aren't read.
void screen_set_on(bool on);

// Draw the pet at animation tick `tick` (one per SCENE_TICK_MS). Call every
// tick; only the part of the screen that changed is sent to the panel.
void screen_draw(const pet_t *pet, uint32_t tick);

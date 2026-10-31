// The pixel-art screen: a small logical canvas scaled up to the AMOLED panel.
#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "pet.h"

// Logical canvas, in "art pixels". 52x64 at 7x = 364x448 on the 368x448
// panel, leaving 2px black bars at each side.
#define SCREEN_ART_W 52
#define SCREEN_ART_H 64
#define SCREEN_ART_SCALE 7

// Start the display and touch. Taps are posted to `events` as PET_EVENT_POKE.
esp_err_t screen_start(QueueHandle_t events);

// Redraw the canvas for the pet's current state.
void screen_draw(const pet_t *pet);

// Power: the charger, the two buttons, and sleeping between looks.
//
// The AXP2101 power chip says whether we're on the charger and whether the
// PWR button was pressed. The BOOT button and the touch chip's interrupt line
// can wake the board from light sleep. The rules for when the screen is on
// live in components/wake.
#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Call after screen_start() (the power chip shares the BSP I2C bus).
// `waker` is the task to notify when a button or tap arrives.
esp_err_t power_start(TaskHandle_t waker);

typedef struct {
    bool charging;    // USB power present (charging or full)
    bool button;      // PWR or BOOT pressed since the last poll
    bool tap;         // the screen was tapped while dark
    int battery_pct;  // 0-100, or -1 if unknown
} power_status_t;

// Read and clear what happened since the last call. A few I2C reads.
power_status_t power_poll(void);

// Dozing: arm the tap and BOOT wake-ups and let the chip light-sleep whenever
// it's idle (Wi-Fi stays connected). Not dozing: run flat out for the screen.
void power_set_dozing(bool dozing);

// Motion sensing: reads the IMU and turns movement into pet events.
#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Start a background task that posts PET_EVENT_SHAKE to `events`.
// Must be called after the display is started (it shares the BSP I2C bus).
esp_err_t motion_start(QueueHandle_t events);

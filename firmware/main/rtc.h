// The board's PCF85063 real-time clock. It runs off the battery through the
// PMU, so it keeps time across reboots. Holds UTC.
#pragma once

#include <stdbool.h>
#include <time.h>

#include "esp_err.h"

// Find the chip. Call after the display is started (shares the BSP I2C bus).
esp_err_t rtc_start(void);

// Read the time. Fails if the chip lost power since it was last set.
esp_err_t rtc_read(time_t *utc);

esp_err_t rtc_write(time_t utc);

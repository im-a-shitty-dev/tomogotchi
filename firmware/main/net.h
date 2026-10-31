// Time and place from Wi-Fi.
//
// Time: London time (with BST). At boot the clock comes from the RTC chip;
// once on Wi-Fi, SNTP corrects it every few hours and writes it back.
// Place: every 5 minutes, scan for nearby networks and work out whether he's
// at home or on the tube (see components/pet/include/place.h).
#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "pet.h"

// Call after the display is started (the RTC shares the BSP I2C bus).
// Keeps going without Wi-Fi credentials: the scans still spot the tube.
esp_err_t net_start(void);

// Where he is now. `now_ms` is the same clock the pet uses. Thread-safe.
pet_place_t net_place(uint32_t now_ms);

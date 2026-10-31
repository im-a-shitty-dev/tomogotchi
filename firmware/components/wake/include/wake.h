// When the screen is on, to save battery.
//
// The screen sleeps after WAKE_TIMEOUT_MS without a touch. A button press or
// a tap on the dark screen wakes it, and a button press while it's on puts it
// to sleep. On the charger it stays on the whole time.
//
// Pure logic: the firmware reports what happened and asks whether the screen
// should be on. `now_ms` is a monotonic clock that may wrap.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WAKE_TIMEOUT_MS 30000u  // Niall: 30 seconds after the last touch

typedef struct {
    bool on;
    bool charging;
    uint32_t last_activity_ms;
} wake_t;

// Starts with the screen on, as at power-up.
void wake_init(wake_t *w, uint32_t now_ms);

// Someone touched or shook him while the screen was on: restart the timeout.
void wake_activity(wake_t *w, uint32_t now_ms);

// A tap on the screen while it was dark.
void wake_tap(wake_t *w, uint32_t now_ms);

// A short press of either button: toggles the screen (but on the charger it
// stays on).
void wake_button(wake_t *w, uint32_t now_ms);

// Plugged in or unplugged. Unplugging restarts the timeout.
void wake_set_charging(wake_t *w, bool charging, uint32_t now_ms);

// Apply the timeout and say whether the screen should be on now.
bool wake_update(wake_t *w, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

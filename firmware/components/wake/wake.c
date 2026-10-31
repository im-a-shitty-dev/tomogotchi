#include "wake.h"

static void turn_on(wake_t *w, uint32_t now_ms)
{
    w->on = true;
    w->last_activity_ms = now_ms;
}

void wake_init(wake_t *w, uint32_t now_ms)
{
    w->charging = false;
    turn_on(w, now_ms);
}

void wake_activity(wake_t *w, uint32_t now_ms)
{
    if (w->on) {
        w->last_activity_ms = now_ms;
    }
}

void wake_tap(wake_t *w, uint32_t now_ms)
{
    turn_on(w, now_ms);
}

void wake_button(wake_t *w, uint32_t now_ms)
{
    if (w->on && !w->charging) {
        w->on = false;
    } else {
        turn_on(w, now_ms);
    }
}

void wake_set_charging(wake_t *w, bool charging, uint32_t now_ms)
{
    if (charging == w->charging) {
        return;
    }
    w->charging = charging;
    turn_on(w, now_ms);  // plugging in lights him up; unplugging gives a full timeout
}

bool wake_update(wake_t *w, uint32_t now_ms)
{
    if (w->charging) {
        w->on = true;
    } else if (w->on && now_ms - w->last_activity_ms >= WAKE_TIMEOUT_MS) {
        w->on = false;
    }
    return w->on;
}

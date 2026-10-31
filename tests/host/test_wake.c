#include <stdio.h>

#include "wake.h"

static int failures = 0;

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,    \
                    __LINE__, #cond);                                 \
            failures++;                                               \
        }                                                             \
    } while (0)

#define SEC 1000u

static void test_sleeps_after_the_timeout(void)
{
    wake_t w;
    wake_init(&w, 0);
    CHECK(wake_update(&w, 0));
    CHECK(wake_update(&w, WAKE_TIMEOUT_MS - 1));
    CHECK(!wake_update(&w, WAKE_TIMEOUT_MS));
}

static void test_touch_keeps_it_on(void)
{
    wake_t w;
    wake_init(&w, 0);
    wake_activity(&w, 20 * SEC);
    CHECK(wake_update(&w, 40 * SEC));
    CHECK(!wake_update(&w, 20 * SEC + WAKE_TIMEOUT_MS));
}

static void test_activity_while_dark_does_not_wake(void)
{
    wake_t w;
    wake_init(&w, 0);
    wake_update(&w, WAKE_TIMEOUT_MS);
    wake_activity(&w, 40 * SEC);  // e.g. a shake in a bag
    CHECK(!wake_update(&w, 40 * SEC));
}

static void test_tap_wakes(void)
{
    wake_t w;
    wake_init(&w, 0);
    wake_update(&w, WAKE_TIMEOUT_MS);
    wake_tap(&w, 100 * SEC);
    CHECK(wake_update(&w, 100 * SEC));
    CHECK(!wake_update(&w, 100 * SEC + WAKE_TIMEOUT_MS));
}

static void test_button_toggles(void)
{
    wake_t w;
    wake_init(&w, 0);
    wake_button(&w, 5 * SEC);
    CHECK(!wake_update(&w, 5 * SEC));
    wake_button(&w, 10 * SEC);
    CHECK(wake_update(&w, 10 * SEC));
    CHECK(!wake_update(&w, 10 * SEC + WAKE_TIMEOUT_MS));
}

static void test_charging_stays_on_for_an_hour(void)
{
    wake_t w;
    wake_init(&w, 0);
    wake_update(&w, WAKE_TIMEOUT_MS);
    wake_set_charging(&w, true, 60 * SEC);  // plugging in wakes it
    CHECK(wake_update(&w, 60 * SEC));
    CHECK(wake_update(&w, 60 * SEC + WAKE_CHARGING_TIMEOUT_MS - 1));
    CHECK(!wake_update(&w, 60 * SEC + WAKE_CHARGING_TIMEOUT_MS));

    // A tap brings it back for another hour; the button can still turn it off.
    uint32_t t = 2 * WAKE_CHARGING_TIMEOUT_MS;
    wake_tap(&w, t);
    CHECK(wake_update(&w, t + 10 * 60 * SEC));
    wake_button(&w, t + 10 * 60 * SEC);
    CHECK(!wake_update(&w, t + 10 * 60 * SEC));

    // Unplugged: on again, with the short timeout.
    t = 3 * WAKE_CHARGING_TIMEOUT_MS;
    wake_set_charging(&w, false, t);
    CHECK(wake_update(&w, t + WAKE_TIMEOUT_MS - 1));
    CHECK(!wake_update(&w, t + WAKE_TIMEOUT_MS));
}

static void test_clock_wrap(void)
{
    wake_t w;
    uint32_t t = 0xFFFFFFFFu - 10 * SEC;
    wake_init(&w, t);
    CHECK(wake_update(&w, t + 20 * SEC));  // wrapped past zero
    CHECK(!wake_update(&w, t + WAKE_TIMEOUT_MS));
}

int main(void)
{
    test_sleeps_after_the_timeout();
    test_touch_keeps_it_on();
    test_activity_while_dark_does_not_wake();
    test_tap_wakes();
    test_button_toggles();
    test_charging_stays_on_for_an_hour();
    test_clock_wrap();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("wake: all passed\n");
    return 0;
}

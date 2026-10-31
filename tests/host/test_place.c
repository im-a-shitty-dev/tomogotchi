#include <stdio.h>
#include <stdlib.h>

#include "place.h"

static int failures = 0;

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,    \
                    __LINE__, #cond);                                 \
            failures++;                                               \
        }                                                             \
    } while (0)

#define MIN (60u * 1000u)
#define SCAN(p, now, ...)                                                  \
    do {                                                                   \
        const char *const list_[] = {__VA_ARGS__};                         \
        place_scan((p), list_, sizeof list_ / sizeof list_[0], (now));     \
    } while (0)

static void test_starts_unknown(void)
{
    place_t p;
    place_init(&p, "TomoHome");
    CHECK(place_now(&p, 0) == PET_PLACE_UNKNOWN);
    place_scan(&p, NULL, 0, 5 * MIN);
    CHECK(place_now(&p, 5 * MIN) == PET_PLACE_UNKNOWN);
}

static void test_tfl_names_ignore_case(void)
{
    CHECK(place_is_tfl_ssid("THREE_WIFI"));
    CHECK(place_is_tfl_ssid("three_wifi"));
    CHECK(place_is_tfl_ssid("EE WiFi-Auto"));
    CHECK(place_is_tfl_ssid("ra1ln3tp0l"));
    CHECK(place_is_tfl_ssid("Ra1In3tP0l"));
    CHECK(!place_is_tfl_ssid("THREE_WIFI2"));
    CHECK(!place_is_tfl_ssid("THREE"));
    CHECK(!place_is_tfl_ssid("_The Cloud"));
    CHECK(!place_is_tfl_ssid(""));
}

static void test_home(void)
{
    place_t p;
    place_init(&p, "TomoHome");
    SCAN(&p, 0, "Neighbour", "tomohome");
    CHECK(place_now(&p, 1 * MIN) == PET_PLACE_HOME);
    // Stale after a missed scan or two: back to the schedule.
    CHECK(place_now(&p, 7 * MIN) == PET_PLACE_UNKNOWN);
    SCAN(&p, 5 * MIN, "Neighbour");
    CHECK(place_now(&p, 5 * MIN) == PET_PLACE_UNKNOWN);
}

static void test_tfl_holds_for_thirty_minutes(void)
{
    place_t p;
    place_init(&p, "TomoHome");
    SCAN(&p, 0, "THREE_WIFI", "Some Cafe");
    CHECK(place_now(&p, 0) == PET_PLACE_COMMUTING);
    SCAN(&p, 5 * MIN);  // in a tunnel
    CHECK(place_now(&p, 29 * MIN) == PET_PLACE_COMMUTING);
    CHECK(place_now(&p, 30 * MIN) == PET_PLACE_UNKNOWN);
    // Another station restarts the hold.
    SCAN(&p, 20 * MIN, "Vodafone_WiFi");
    CHECK(place_now(&p, 45 * MIN) == PET_PLACE_COMMUTING);
}

static void test_home_ends_commute_early(void)
{
    place_t p;
    place_init(&p, "TomoHome");
    SCAN(&p, 0, "WiFi_Extra");
    SCAN(&p, 5 * MIN, "TomoHome");
    CHECK(place_now(&p, 5 * MIN) == PET_PLACE_HOME);
    // Walking out of range again doesn't bring the commute back.
    SCAN(&p, 10 * MIN, "Neighbour");
    CHECK(place_now(&p, 10 * MIN) == PET_PLACE_UNKNOWN);
}

static void test_home_beats_hotspot_in_same_scan(void)
{
    place_t p;
    place_init(&p, "TomoHome");
    SCAN(&p, 0, "THREE_WIFI", "TomoHome");
    CHECK(place_now(&p, 0) == PET_PLACE_HOME);
}

static void test_no_home_configured(void)
{
    place_t p;
    place_init(&p, NULL);
    SCAN(&p, 0, "", "Anything");
    CHECK(place_now(&p, 0) == PET_PLACE_UNKNOWN);
    SCAN(&p, 0, "THREE_WIFI");
    CHECK(place_now(&p, 0) == PET_PLACE_COMMUTING);
}

static void test_survives_clock_wrap(void)
{
    place_t p;
    place_init(&p, "TomoHome");
    uint32_t t = 0xFFFFFFFFu - MIN;
    SCAN(&p, t, "THREE_WIFI");
    CHECK(place_now(&p, t + 10 * MIN) == PET_PLACE_COMMUTING);
    CHECK(place_now(&p, t + 31 * MIN) == PET_PLACE_UNKNOWN);
}

int main(void)
{
    test_starts_unknown();
    test_tfl_names_ignore_case();
    test_home();
    test_tfl_holds_for_thirty_minutes();
    test_home_ends_commute_early();
    test_home_beats_hotspot_in_same_scan();
    test_no_home_configured();
    test_survives_clock_wrap();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all place tests passed\n");
    return EXIT_SUCCESS;
}

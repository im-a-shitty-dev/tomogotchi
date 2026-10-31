#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pet.h"

static int failures = 0;

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,    \
                    __LINE__, #cond);                                 \
            failures++;                                               \
        }                                                             \
    } while (0)

// Days in a plain week of March 2026: Mon 9th ... Sun 15th.
enum { SUN = 0, MON = 1, TUE = 2, WED = 3, THU = 4, FRI = 5, SAT = 6 };

static pet_clock_t at(int wday, int hour, int minute)
{
    return (pet_clock_t){.month = 3, .mday = 8 + (wday == SUN ? 7 : wday), .wday = wday, .hour = hour, .minute = minute};
}

static pet_mood_t mood_at(int wday, int hour, int minute)
{
    pet_clock_t c = at(wday, hour, minute);
    return pet_mood_for(&c);
}

static const pet_event_t TAP = {.type = PET_INPUT_TAP};
static const pet_event_t DOUBLE_TAP = {.type = PET_INPUT_DOUBLE_TAP};
static const pet_event_t SHAKE = {.type = PET_INPUT_SHAKE};
static const pet_event_t LONG_PRESS = {.type = PET_INPUT_LONG_PRESS};
static const pet_event_t SWIPE = {.type = PET_INPUT_SWIPE, .direction = 1};
static const pet_event_t SWIPE_BACK = {.type = PET_INPUT_SWIPE, .direction = -1};

static pet_event_t give(pet_item_t item)
{
    return (pet_event_t){.type = PET_INPUT_GIVE, .item = item};
}

// A pet that has been ticked once at the given time.
static pet_t pet_at(int wday, int hour, int minute)
{
    pet_t pet;
    pet_init(&pet, 1);
    pet_clock_t c = at(wday, hour, minute);
    pet_tick(&pet, &c, 0);
    return pet;
}

static bool tick(pet_t *pet, int wday, int hour, int minute, uint32_t now_ms)
{
    pet_clock_t c = at(wday, hour, minute);
    return pet_tick(pet, &c, now_ms);
}

// --- Schedule ------------------------------------------------------------------

static void test_weekday_schedule(void)
{
    CHECK(mood_at(TUE, 4, 59) == PET_MOOD_ASLEEP);
    CHECK(mood_at(TUE, 5, 0) == PET_MOOD_GRUMPY);
    CHECK(mood_at(TUE, 6, 0) == PET_MOOD_COMMUTE);
    CHECK(mood_at(TUE, 7, 0) == PET_MOOD_DEADPAN);
    CHECK(mood_at(TUE, 15, 29) == PET_MOOD_DEADPAN);
    CHECK(mood_at(TUE, 15, 30) == PET_MOOD_COMMUTE);
    CHECK(mood_at(TUE, 17, 0) == PET_MOOD_ME_TIME);
    CHECK(mood_at(TUE, 19, 30) == PET_MOOD_NIGHT);
    CHECK(mood_at(TUE, 21, 0) == PET_MOOD_ASLEEP);
}

static void test_friday_horror_night(void)
{
    CHECK(mood_at(FRI, 18, 59) == PET_MOOD_ME_TIME);
    CHECK(mood_at(FRI, 19, 0) == PET_MOOD_SPOOKY);
    CHECK(mood_at(FRI, 23, 30) == PET_MOOD_SPOOKY);
    CHECK(mood_at(SAT, 1, 0) == PET_MOOD_NIGHT);  // Saturday small hours
}

static void test_weekend_schedule(void)
{
    CHECK(mood_at(SAT, 11, 29) == PET_MOOD_ASLEEP);
    CHECK(mood_at(SAT, 11, 30) == PET_MOOD_GRUMPY);
    CHECK(mood_at(SAT, 13, 0) == PET_MOOD_DEADPAN);
    CHECK(mood_at(SAT, 19, 0) == PET_MOOD_NIGHT);
    CHECK(mood_at(SUN, 1, 0) == PET_MOOD_SPOOKY);  // Saturday night running late
    CHECK(mood_at(SUN, 13, 0) == PET_MOOD_ME_TIME);
    CHECK(mood_at(SUN, 20, 59) == PET_MOOD_ME_TIME);
    CHECK(mood_at(SUN, 21, 0) == PET_MOOD_NIGHT);
    CHECK(mood_at(SUN, 22, 0) == PET_MOOD_ASLEEP);
}

static void test_halloween_is_spooky_while_awake(void)
{
    pet_clock_t c = {.month = 10, .mday = 31, .wday = SAT, .hour = 14, .minute = 0};
    CHECK(pet_mood_for(&c) == PET_MOOD_SPOOKY);
    c.hour = 8;  // still in bed
    CHECK(pet_mood_for(&c) == PET_MOOD_ASLEEP);
}

static void test_me_time_scene(void)
{
    pet_clock_t c = at(TUE, 18, 0);
    CHECK(pet_scene_for(&c) == PET_SCENE_BATH);
    c = at(SUN, 15, 0);
    CHECK(pet_scene_for(&c) == PET_SCENE_COUCH);
    c.month = 7;
    CHECK(pet_scene_for(&c) == PET_SCENE_BEACH);
    c = at(TUE, 12, 0);
    CHECK(pet_scene_for(&c) == PET_SCENE_NONE);
}

static void test_tick_follows_the_clock(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    CHECK(pet_view(&pet).mood == PET_MOOD_DEADPAN);
    CHECK(!tick(&pet, TUE, 12, 1, 10));  // nothing to redraw
    CHECK(tick(&pet, TUE, 15, 30, 20));  // time to go home
    CHECK(pet_view(&pet).mood == PET_MOOD_COMMUTE);
}

// --- View ----------------------------------------------------------------------

static void test_view_per_mood(void)
{
    pet_t pet = pet_at(TUE, 5, 30);
    pet_view_t v = pet_view(&pet);
    CHECK(v.face == PET_FACE_GRUMPY && v.cup && v.bedhead);

    pet = pet_at(TUE, 6, 30);
    v = pet_view(&pet);
    CHECK(v.outfit == PET_OUTFIT_COAT && v.phone);

    pet = pet_at(TUE, 18, 0);
    v = pet_view(&pet);
    CHECK(v.scene == PET_SCENE_BATH && v.outfit == PET_OUTFIT_HIDDEN && v.face == PET_FACE_HAPPY);

    pet = pet_at(SUN, 15, 0);
    v = pet_view(&pet);
    CHECK(v.outfit == PET_OUTFIT_LYING && v.face == PET_FACE_READ);

    pet = pet_at(TUE, 20, 0);
    v = pet_view(&pet);
    CHECK(v.outfit == PET_OUTFIT_PJS && v.device == PET_DEVICE_PHONE && v.face == PET_FACE_SCREEN);

    pet = pet_at(TUE, 23, 0);
    v = pet_view(&pet);
    CHECK(v.face == PET_FACE_SLEEP && v.line == NULL);
    CHECK(v.happiness == PET_HAPPINESS_START);
}

// --- Taps and shakes -----------------------------------------------------------

static void test_tap_smirks_when_deadpan(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    CHECK(pet_handle_event(&pet, TAP, 100));
    pet_view_t v = pet_view(&pet);
    CHECK(v.face == PET_FACE_SMIRK);
    CHECK(v.line != NULL);
    CHECK(v.happiness == PET_HAPPINESS_START);  // he doesn't mind
}

static void test_tap_annoys_when_grumpy_and_costs_a_heart(void)
{
    pet_t pet = pet_at(TUE, 5, 30);
    pet_handle_event(&pet, TAP, 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.face == PET_FACE_ANNOYED);
    CHECK(v.happiness == PET_HAPPINESS_START - 1);
}

static void test_shake_makes_him_dizzy(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_handle_event(&pet, SHAKE, 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.face == PET_FACE_DIZZY && v.shake);
    CHECK(v.happiness == PET_HAPPINESS_START - 1);
}

static void test_reaction_wears_off(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_handle_event(&pet, TAP, 1000);
    CHECK(!tick(&pet, TUE, 12, 0, 1100));
    CHECK(pet_view(&pet).line != NULL);
    CHECK(tick(&pet, TUE, 12, 0, 1000 + 5000));
    CHECK(pet_view(&pet).line == NULL);
    CHECK(pet_view(&pet).face == PET_FACE_BLANK);
}

static void test_reaction_survives_clock_wrap(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    uint32_t start = UINT32_MAX - 100;
    pet_handle_event(&pet, TAP, start);
    CHECK(!tick(&pet, TUE, 12, 0, start + 50));
    CHECK(pet_view(&pet).line != NULL);
    CHECK(tick(&pet, TUE, 12, 0, start + 5000));
}

static void test_tap_in_spooky_swaps_costume(void)
{
    pet_t pet = pet_at(FRI, 20, 0);
    pet_costume_t before = pet_view(&pet).costume;
    pet_handle_event(&pet, TAP, 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.outfit == PET_OUTFIT_COSTUME);
    CHECK(v.costume == (before + 1) % PET_COSTUME_COUNT);
}

static void test_tap_at_night_swaps_device(void)
{
    pet_t pet = pet_at(TUE, 20, 0);
    pet_handle_event(&pet, TAP, 100);
    CHECK(pet_view(&pet).device == PET_DEVICE_SWITCH);
    pet_handle_event(&pet, TAP, 5000);
    CHECK(pet_view(&pet).device == PET_DEVICE_PHONE);
}

// --- Turning around ------------------------------------------------------------

static void test_double_tap_turns_him_and_tap_turns_him_back(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_handle_event(&pet, DOUBLE_TAP, 100);
    CHECK(pet_view(&pet).back);
    pet_handle_event(&pet, TAP, 200);
    CHECK(!pet_view(&pet).back);
    CHECK(pet_view(&pet).line != NULL);  // "…what."
}

static void test_turning_in_the_bath_stands_him_up(void)
{
    pet_t pet = pet_at(TUE, 18, 0);
    pet_handle_event(&pet, DOUBLE_TAP, 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.back && v.outfit == PET_OUTFIT_BATH);
}

static void test_no_turning_when_asleep_or_lying_down(void)
{
    pet_t pet = pet_at(TUE, 23, 0);
    pet_handle_event(&pet, DOUBLE_TAP, 100);
    CHECK(!pet_view(&pet).back);

    pet = pet_at(SUN, 15, 0);
    pet_handle_event(&pet, DOUBLE_TAP, 100);
    CHECK(!pet_view(&pet).back);
    CHECK(pet_view(&pet).line != NULL);  // "comfy. no."
}

static void test_mood_change_turns_him_round(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_handle_event(&pet, DOUBLE_TAP, 100);
    tick(&pet, TUE, 15, 30, 200);
    CHECK(!pet_view(&pet).back);
}

// --- Snacks --------------------------------------------------------------------

static void test_long_press_opens_snack_picker(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_handle_event(&pet, LONG_PRESS, 100);
    CHECK(pet_view(&pet).menu_open);
    CHECK(tick(&pet, TUE, 12, 0, 100 + PET_MENU_MS));
    CHECK(!pet_view(&pet).menu_open);
}

static void test_tap_closes_snack_picker(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_handle_event(&pet, LONG_PRESS, 100);
    pet_handle_event(&pet, TAP, 200);
    CHECK(!pet_view(&pet).menu_open);
    CHECK(pet_view(&pet).line == NULL);  // just closed it, no reaction
}

static void test_snacks_always_help(void)
{
    pet_item_t items[] = {PET_ITEM_GUMMY, PET_ITEM_CRISPS, PET_ITEM_COFFEE};
    int times[][2] = {{TUE, 6}, {TUE, 12}, {TUE, 18}, {TUE, 20}, {FRI, 20}, {SUN, 15}};
    for (size_t t = 0; t < sizeof(times) / sizeof(times[0]); t++) {
        for (size_t i = 0; i < 3; i++) {
            pet_t pet = pet_at(times[t][0], times[t][1], 0);
            pet_handle_event(&pet, LONG_PRESS, 50);
            pet_handle_event(&pet, give(items[i]), 100);
            pet_view_t v = pet_view(&pet);
            CHECK(v.happiness == PET_HAPPINESS_START + 1);
            CHECK(v.item == items[i]);
            CHECK(v.line != NULL);
            CHECK(!v.menu_open);
            CHECK(!v.phone && v.device == PET_DEVICE_NONE);  // hands full
        }
    }
}

static void test_morning_coffee_helps_most(void)
{
    pet_t pet = pet_at(TUE, 5, 30);
    pet_handle_event(&pet, give(PET_ITEM_COFFEE), 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.happiness == PET_HAPPINESS_START + 2);
    CHECK(!v.cup);  // holding the new coffee instead
    CHECK(strcmp(v.line, "ok. alive.") == 0 || strcmp(v.line, "…おはよ") == 0);
}

static void test_happiness_is_capped(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    for (int i = 0; i < 10; i++) {
        pet_handle_event(&pet, give(PET_ITEM_GUMMY), 100 + i);
    }
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_MAX);
}

static void test_snack_while_asleep(void)
{
    pet_t pet = pet_at(TUE, 23, 0);
    pet_handle_event(&pet, give(PET_ITEM_GUMMY), 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.face == PET_FACE_SLEEP && v.item == PET_ITEM_NONE);
    CHECK(v.happiness == PET_HAPPINESS_START + 1);
}

// --- Being ignored and sulking -------------------------------------------------

static void test_ignored_he_gets_less_happy(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    tick(&pet, TUE, 12, 0, PET_IGNORED_MS - 1);
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START);
    CHECK(tick(&pet, TUE, 12, 0, PET_IGNORED_MS));
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START - 1);
}

static void test_no_decay_while_asleep(void)
{
    pet_t pet = pet_at(TUE, 23, 0);
    tick(&pet, TUE, 23, 0, PET_IGNORED_MS * 3);
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START);
}

static void test_interaction_resets_decay(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_handle_event(&pet, TAP, PET_IGNORED_MS - 10);
    tick(&pet, TUE, 12, 0, PET_IGNORED_MS);
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START);
}

// Drive happiness to zero with shakes.
static pet_t sulking_pet(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    for (int i = 0; i < PET_HAPPINESS_START; i++) {
        pet_handle_event(&pet, SHAKE, 100 + i);
    }
    return pet;
}

static void test_zero_happiness_and_he_sulks(void)
{
    pet_t pet = sulking_pet();
    pet_view_t v = pet_view(&pet);
    CHECK(v.sulking && v.back && v.happiness == 0);

    pet_handle_event(&pet, DOUBLE_TAP, 1000);  // won't turn round
    CHECK(pet_view(&pet).back);
    pet_handle_event(&pet, TAP, 2000);
    CHECK(pet_view(&pet).back);
    pet_handle_event(&pet, SWIPE, 3000);  // won't dress up either
    CHECK(pet_view(&pet).back && pet_view(&pet).outfit == PET_OUTFIT_CASUAL);
}

static void test_sulk_ends_after_food_and_a_quiet_bath(void)
{
    pet_t pet = sulking_pet();
    pet_handle_event(&pet, give(PET_ITEM_GUMMY), 1000);
    pet_handle_event(&pet, give(PET_ITEM_CRISPS), 2000);
    pet_view_t v = pet_view(&pet);
    CHECK(v.sulking && v.back && v.mood == PET_MOOD_DEADPAN);  // not fed enough yet

    pet_handle_event(&pet, give(PET_ITEM_COFFEE), 3000);
    v = pet_view(&pet);
    CHECK(v.sulking && !v.back);
    CHECK(v.mood == PET_MOOD_ME_TIME && v.scene == PET_SCENE_BATH);

    // Poking him in the bath starts the clock again.
    uint32_t poked = 3000 + PET_SULK_BATH_MS - 1000;
    tick(&pet, TUE, 12, 0, poked - 1);
    pet_handle_event(&pet, TAP, poked);
    tick(&pet, TUE, 12, 0, 3000 + PET_SULK_BATH_MS);
    CHECK(pet_view(&pet).sulking);

    CHECK(tick(&pet, TUE, 12, 0, poked + PET_SULK_BATH_MS));
    v = pet_view(&pet);
    CHECK(!v.sulking && !v.back && v.mood == PET_MOOD_DEADPAN);
    CHECK(v.line != NULL);  // forgives you
}

static void test_no_sulk_from_sleepy_pokes(void)
{
    pet_t pet = pet_at(TUE, 23, 0);
    for (int i = 0; i < 5; i++) {
        pet_handle_event(&pet, TAP, 100 + i);
    }
    CHECK(pet_view(&pet).happiness == 0);
    CHECK(!pet_view(&pet).sulking);
}

// --- Costumes, any time --------------------------------------------------------

static void test_swipe_dresses_him_up_any_time(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_handle_event(&pet, SWIPE, 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.mood == PET_MOOD_SPOOKY && v.outfit == PET_OUTFIT_COSTUME);
    CHECK(v.costume == PET_COSTUME_DRACULA);

    pet_handle_event(&pet, SWIPE, 200);
    CHECK(pet_view(&pet).costume == PET_COSTUME_CHIIKAWA);
    pet_handle_event(&pet, SWIPE_BACK, 300);
    CHECK(pet_view(&pet).costume == PET_COSTUME_DRACULA);

    // Past the start: back to normal clothes.
    pet_handle_event(&pet, SWIPE_BACK, 400);
    CHECK(pet_view(&pet).mood == PET_MOOD_DEADPAN);
    // And the next swipe back starts from the last costume.
    pet_handle_event(&pet, SWIPE_BACK, 500);
    CHECK(pet_view(&pet).costume == PET_COSTUME_SYLVEON);
}

static void test_swiping_through_every_costume(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    for (int i = 0; i < PET_COSTUME_COUNT; i++) {
        pet_handle_event(&pet, SWIPE, 100 + i);
        CHECK(pet_view(&pet).costume == (pet_costume_t)i);
    }
    pet_handle_event(&pet, SWIPE, 1000);
    CHECK(pet_view(&pet).mood == PET_MOOD_DEADPAN);
}

static void test_costume_stays_on_through_the_day_and_off_for_bed(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_handle_event(&pet, SWIPE, 100);
    tick(&pet, TUE, 18, 0, 200);
    CHECK(pet_view(&pet).mood == PET_MOOD_SPOOKY);
    tick(&pet, TUE, 21, 0, 300);
    CHECK(pet_view(&pet).mood == PET_MOOD_ASLEEP);
    tick(&pet, WED, 5, 0, 400);
    CHECK(pet_view(&pet).mood == PET_MOOD_GRUMPY);
}

static void test_scheduled_spooky_just_wraps(void)
{
    pet_t pet = pet_at(FRI, 20, 0);
    pet_handle_event(&pet, SWIPE_BACK, 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.mood == PET_MOOD_SPOOKY && v.costume == PET_COSTUME_SYLVEON);
}

static void test_no_dressing_up_while_asleep(void)
{
    pet_t pet = pet_at(TUE, 23, 0);
    pet_handle_event(&pet, SWIPE, 100);
    CHECK(pet_view(&pet).mood == PET_MOOD_ASLEEP);
}

// --- Misc ----------------------------------------------------------------------

static void test_same_seed_same_lines(void)
{
    pet_t a = pet_at(TUE, 12, 0), b = pet_at(TUE, 12, 0);
    for (int i = 0; i < 5; i++) {
        pet_handle_event(&a, TAP, 100 + i);
        pet_handle_event(&b, TAP, 100 + i);
        CHECK(pet_view(&a).line == pet_view(&b).line);
    }
}

static void test_names(void)
{
    CHECK(strcmp(pet_mood_name(PET_MOOD_GRUMPY), "grumpy") == 0);
    CHECK(strcmp(pet_mood_name(PET_MOOD_COUNT), "unknown") == 0);
    CHECK(strcmp(pet_costume_name(PET_COSTUME_BARREL), "barrel") == 0);
    CHECK(pet_costume_is_cute(PET_COSTUME_CHIIKAWA));
    CHECK(!pet_costume_is_cute(PET_COSTUME_JASON));
}

int main(void)
{
    test_weekday_schedule();
    test_friday_horror_night();
    test_weekend_schedule();
    test_halloween_is_spooky_while_awake();
    test_me_time_scene();
    test_tick_follows_the_clock();
    test_view_per_mood();
    test_tap_smirks_when_deadpan();
    test_tap_annoys_when_grumpy_and_costs_a_heart();
    test_shake_makes_him_dizzy();
    test_reaction_wears_off();
    test_reaction_survives_clock_wrap();
    test_tap_in_spooky_swaps_costume();
    test_tap_at_night_swaps_device();
    test_double_tap_turns_him_and_tap_turns_him_back();
    test_turning_in_the_bath_stands_him_up();
    test_no_turning_when_asleep_or_lying_down();
    test_mood_change_turns_him_round();
    test_long_press_opens_snack_picker();
    test_tap_closes_snack_picker();
    test_snacks_always_help();
    test_morning_coffee_helps_most();
    test_happiness_is_capped();
    test_snack_while_asleep();
    test_ignored_he_gets_less_happy();
    test_no_decay_while_asleep();
    test_interaction_resets_decay();
    test_zero_happiness_and_he_sulks();
    test_sulk_ends_after_food_and_a_quiet_bath();
    test_no_sulk_from_sleepy_pokes();
    test_swipe_dresses_him_up_any_time();
    test_swiping_through_every_costume();
    test_costume_stays_on_through_the_day_and_off_for_bed();
    test_scheduled_spooky_just_wraps();
    test_no_dressing_up_while_asleep();
    test_same_seed_same_lines();
    test_names();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all pet tests passed\n");
    return EXIT_SUCCESS;
}

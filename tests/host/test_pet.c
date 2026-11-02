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
static const pet_event_t SWIPE_UP = {.type = PET_INPUT_SWIPE_UP};

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
    CHECK(mood_at(TUE, 7, 0) == PET_MOOD_WORK);
    CHECK(mood_at(TUE, 15, 29) == PET_MOOD_WORK);
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
    c = at(SAT, 14, 0);
    CHECK(pet_scene_for(&c) == PET_SCENE_NONE);
}

static void test_tick_follows_the_clock(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    CHECK(pet_view(&pet).mood == PET_MOOD_WORK);
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

static void test_at_work(void)
{
    pet_t pet = pet_at(TUE, 12, 0);
    pet_view_t v = pet_view(&pet);
    CHECK(v.mood == PET_MOOD_WORK && v.outfit == PET_OUTFIT_WORK);
    CHECK(!v.phone && v.face == PET_FACE_BLANK);  // no phone on the shop floor

    pet_handle_event(&pet, TAP, 100);  // a poke: deadpan reply, no cost
    v = pet_view(&pet);
    CHECK(v.line != NULL && v.face == PET_FACE_BLANK);
    CHECK(v.happiness == PET_HAPPINESS_START);

    pet_handle_event(&pet, SHAKE, 2000);
    v = pet_view(&pet);
    CHECK(v.shake && v.face == PET_FACE_ANNOYED && v.happiness == PET_HAPPINESS_START - 1);

    pet = pet_at(SAT, 12, 30);  // weekends: no work
    CHECK(pet_view(&pet).mood != PET_MOOD_WORK);
    CHECK(pet_mood_name(PET_MOOD_WORK)[0] == 'w');
}

static void test_tap_smirks_when_deadpan(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
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
    CHECK(v.happiness == PET_HAPPINESS_START);  // one poke is free...
    for (int i = 1; i < PET_POKES_PER_HEART; i++) pet_handle_event(&pet, TAP, 100 + i);
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START - 1);  // ...every third costs a heart
}

static void test_fumbled_double_tap_in_the_bath_is_cheap(void)
{
    // A double-tap that comes in as two taps costs at most one heart.
    pet_t pet = pet_at(TUE, 18, 0);  // bath by the schedule
    pet_handle_event(&pet, TAP, 100);
    pet_handle_event(&pet, TAP, 500);
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START);
    pet_handle_event(&pet, TAP, 900);
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START - 1);
}

static void test_shake_makes_him_dizzy(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_handle_event(&pet, SHAKE, 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.face == PET_FACE_DIZZY && v.shake);
    CHECK(v.happiness == PET_HAPPINESS_START - 1);
}

static void test_reaction_wears_off(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_handle_event(&pet, TAP, 1000);
    CHECK(!tick(&pet, SAT, 14, 0, 1100));
    CHECK(pet_view(&pet).line != NULL);
    CHECK(tick(&pet, SAT, 14, 0, 1000 + 5000));
    CHECK(pet_view(&pet).line == NULL);
    CHECK(pet_view(&pet).face == PET_FACE_BLANK);
}

static void test_reaction_survives_clock_wrap(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    uint32_t start = UINT32_MAX - 100;
    pet_handle_event(&pet, TAP, start);
    CHECK(!tick(&pet, SAT, 14, 0, start + 50));
    CHECK(pet_view(&pet).line != NULL);
    CHECK(tick(&pet, SAT, 14, 0, start + 5000));
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
    pet_t pet = pet_at(SAT, 14, 0);
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
    pet_t pet = pet_at(SAT, 14, 0);
    pet_handle_event(&pet, DOUBLE_TAP, 100);
    tick(&pet, TUE, 15, 30, 200);
    CHECK(!pet_view(&pet).back);
}

// --- Snacks --------------------------------------------------------------------

static void test_long_press_opens_snack_picker(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_handle_event(&pet, LONG_PRESS, 100);
    CHECK(pet_view(&pet).menu_open);
    CHECK(tick(&pet, SAT, 14, 0, 100 + PET_MENU_MS));
    CHECK(!pet_view(&pet).menu_open);
}

static void test_tap_closes_snack_picker(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
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
    pet_t pet = pet_at(SAT, 14, 0);
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
    pet_t pet = pet_at(SAT, 14, 0);
    tick(&pet, SAT, 14, 0, PET_IGNORED_MS - 1);
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START);
    CHECK(tick(&pet, SAT, 14, 0, PET_IGNORED_MS));
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
    pet_t pet = pet_at(SAT, 14, 0);
    pet_handle_event(&pet, TAP, PET_IGNORED_MS - 10);
    tick(&pet, SAT, 14, 0, PET_IGNORED_MS);
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START);
}

// Drive happiness to zero with shakes.
static pet_t sulking_pet(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
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

    // Poking him in the bath makes him wait a minute longer.
    uint32_t poked = 3000 + PET_SULK_BATH_MS - 1000;
    tick(&pet, SAT, 14, 0, poked - 1);
    pet_handle_event(&pet, TAP, poked);
    tick(&pet, SAT, 14, 0, 3000 + PET_SULK_BATH_MS);
    CHECK(pet_view(&pet).sulking);
    tick(&pet, SAT, 14, 0, 3000 + PET_SULK_BATH_MS + PET_SULK_POKE_MS - 1);
    CHECK(pet_view(&pet).sulking);

    CHECK(tick(&pet, SAT, 14, 0, 3000 + PET_SULK_BATH_MS + PET_SULK_POKE_MS));
    v = pet_view(&pet);
    CHECK(!v.sulking && !v.back && v.mood == PET_MOOD_DEADPAN);
    CHECK(v.line != NULL);  // forgives you
}

static void test_pokes_in_the_sulk_bath_add_up_but_never_past_a_full_wait(void)
{
    pet_t pet = sulking_pet();
    for (int i = 0; i < PET_SULK_FED; i++) pet_handle_event(&pet, give(PET_ITEM_GUMMY), 1000 + i);
    CHECK(pet.sulk_bathing);
    // Lots of pokes straight away: still just one full wait from now.
    for (int i = 0; i < 20; i++) pet_handle_event(&pet, i % 2 ? SHAKE : TAP, 2000 + i);
    tick(&pet, SAT, 14, 0, 2019 + PET_SULK_BATH_MS - 1);
    CHECK(pet_view(&pet).sulking);
    tick(&pet, SAT, 14, 0, 2019 + PET_SULK_BATH_MS);
    CHECK(!pet_view(&pet).sulking);
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
    pet_t pet = pet_at(SAT, 14, 0);
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
    pet_t pet = pet_at(SAT, 14, 0);
    for (int i = 0; i < PET_COSTUME_COUNT; i++) {
        pet_handle_event(&pet, SWIPE, 100 + i);
        CHECK(pet_view(&pet).costume == (pet_costume_t)i);
    }
    pet_handle_event(&pet, SWIPE, 1000);
    CHECK(pet_view(&pet).mood == PET_MOOD_DEADPAN);
}

static void test_costume_stays_on_through_the_day_and_off_for_bed(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
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

// --- Scenes, any time ----------------------------------------------------------

static void test_swipe_up_cycles_scenes_then_back_to_schedule(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_handle_event(&pet, SWIPE_UP, 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.mood == PET_MOOD_ME_TIME && v.scene == PET_SCENE_BATH);
    pet_handle_event(&pet, SWIPE_UP, 200);
    CHECK(pet_view(&pet).scene == PET_SCENE_COUCH);
    CHECK(pet_view(&pet).outfit == PET_OUTFIT_LYING);
    pet_handle_event(&pet, SWIPE_UP, 300);
    CHECK(pet_view(&pet).scene == PET_SCENE_BEACH);
    pet_handle_event(&pet, SWIPE_UP, 400);
    v = pet_view(&pet);
    CHECK(v.mood == PET_MOOD_DEADPAN && v.scene == PET_SCENE_NONE);
}

static void test_swipe_up_in_scheduled_me_time_moves_on(void)
{
    pet_t pet = pet_at(TUE, 18, 0);  // bath by the schedule
    pet_handle_event(&pet, SWIPE_UP, 100);
    CHECK(pet_view(&pet).scene == PET_SCENE_COUCH);
    pet_handle_event(&pet, SWIPE_UP, 200);
    CHECK(pet_view(&pet).scene == PET_SCENE_BEACH);
    pet_handle_event(&pet, SWIPE_UP, 300);  // back to the schedule: bath
    CHECK(pet_view(&pet).scene == PET_SCENE_BATH && pet.chosen_scene == PET_SCENE_NONE);
}

static void test_swipe_up_reaches_the_bath_on_a_lazy_sunday(void)
{
    // Sunday afternoon is couch me-time. It used to bounce couch, beach,
    // couch, beach and never reach the bath.
    pet_t pet = pet_at(SUN, 14, 0);
    CHECK(pet_view(&pet).scene == PET_SCENE_COUCH);
    pet_handle_event(&pet, SWIPE_UP, 100);
    CHECK(pet_view(&pet).scene == PET_SCENE_BATH);
    pet_handle_event(&pet, SWIPE_UP, 200);
    CHECK(pet_view(&pet).scene == PET_SCENE_COUCH);  // picked, not scheduled
    CHECK(pet.chosen_scene == PET_SCENE_COUCH);
    pet_handle_event(&pet, SWIPE_UP, 300);
    CHECK(pet_view(&pet).scene == PET_SCENE_BEACH);
    pet_handle_event(&pet, SWIPE_UP, 400);
    CHECK(pet_view(&pet).scene == PET_SCENE_COUCH && pet.chosen_scene == PET_SCENE_NONE);
    pet_handle_event(&pet, SWIPE_UP, 500);
    CHECK(pet_view(&pet).scene == PET_SCENE_BATH);
}

static void test_swipe_up_takes_the_costume_off(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_handle_event(&pet, SWIPE, 100);
    CHECK(pet_view(&pet).mood == PET_MOOD_SPOOKY);
    pet_handle_event(&pet, SWIPE_UP, 200);
    pet_view_t v = pet_view(&pet);
    CHECK(v.mood == PET_MOOD_ME_TIME && v.scene == PET_SCENE_BATH);
    pet_handle_event(&pet, SWIPE, 300);  // and a costume swipe still works
    CHECK(pet_view(&pet).mood == PET_MOOD_SPOOKY);
}

static void test_chosen_scene_lasts_until_bed(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_handle_event(&pet, SWIPE_UP, 100);
    tick(&pet, TUE, 15, 30, 200);  // commute time
    CHECK(pet_view(&pet).scene == PET_SCENE_BATH);
    tick(&pet, TUE, 21, 0, 300);
    CHECK(pet_view(&pet).mood == PET_MOOD_ASLEEP);
    tick(&pet, WED, 7, 0, 400);
    CHECK(pet_view(&pet).mood == PET_MOOD_WORK);
}

static void test_no_scene_swipe_while_asleep_or_sulking(void)
{
    pet_t pet = pet_at(TUE, 23, 0);
    pet_handle_event(&pet, SWIPE_UP, 100);
    CHECK(pet_view(&pet).mood == PET_MOOD_ASLEEP);
    tick(&pet, WED, 7, 0, 200);
    CHECK(pet_view(&pet).mood == PET_MOOD_WORK);

    pet = sulking_pet();
    pet_handle_event(&pet, SWIPE_UP, 1000);
    CHECK(pet_view(&pet).back && pet_view(&pet).mood == PET_MOOD_DEADPAN);
}

static void test_sulking_on_the_couch_gets_him_up(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_handle_event(&pet, SWIPE_UP, 100);
    pet_handle_event(&pet, SWIPE_UP, 200);  // couch
    for (int i = 0; i < PET_HAPPINESS_START; i++) {
        pet_handle_event(&pet, SHAKE, 300 + i);
    }
    pet_view_t v = pet_view(&pet);
    CHECK(v.sulking && v.back && v.mood == PET_MOOD_DEADPAN);
}

// --- Misc ----------------------------------------------------------------------

// --- Shopping bags -------------------------------------------------------------

#define CARRY_MS (24u * 110u)

static pet_event_t clear_bag(int i)
{
    return (pet_event_t){.type = PET_INPUT_CLEAR_BAG, .bag = i};
}

static void put_bags(pet_t *pet, int n)
{
    for (int i = 0; i < n; i++) pet->bags[i] = (pet_bag_t)(i % PET_BAG_COUNT);
    pet->bag_count = (uint8_t)n;
}

static void test_weekend_shopping_trip_brings_bags_home(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_set_place(&pet, PET_PLACE_HOME);
    tick(&pet, SAT, 14, 0, 1000);
    pet_set_place(&pet, PET_PLACE_AWAY);
    tick(&pet, SAT, 14, 5, 2000);
    CHECK(pet_view(&pet).mood == PET_MOOD_DEADPAN);  // out doesn't change his mood
    pet_set_place(&pet, PET_PLACE_COMMUTING);  // tube home: still the same trip
    tick(&pet, SAT, 14, 30, 2000 + PET_SHOP_TRIP_MS - 1000);
    pet_set_place(&pet, PET_PLACE_HOME);
    uint32_t back = 2000 + PET_SHOP_TRIP_MS;
    CHECK(tick(&pet, SAT, 14, 35, back));

    // He walks in carrying them, with a line...
    pet_view_t v = pet_view(&pet);
    CHECK(v.carry_count >= 1 && v.carry_count <= PET_BAGS_CARRIED);
    CHECK(v.bag_count == 0 && v.line != NULL && v.face == PET_FACE_SMIRK);
    int bought = pet.bought_count;
    CHECK(bought >= 1 && bought <= PET_BAGS_PER_TRIP);
    CHECK(!tick(&pet, SAT, 14, 35, back + CARRY_MS - 1) || pet_view(&pet).carry_count > 0);
    // ...then puts them on the floor.
    CHECK(tick(&pet, SAT, 14, 35, back + CARRY_MS));
    v = pet_view(&pet);
    CHECK(v.carry_count == 0 && v.bag_count == bought);
    for (int i = 0; i < v.bag_count; i++) CHECK(v.bags[i] < PET_BAG_COUNT);
}

static void test_a_quick_pop_out_is_not_shopping(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_set_place(&pet, PET_PLACE_AWAY);
    tick(&pet, SAT, 14, 0, 1000);
    pet_set_place(&pet, PET_PLACE_HOME);
    tick(&pet, SAT, 14, 10, 1000 + PET_SHOP_TRIP_MS - 1);
    CHECK(pet.bought_count == 0 && pet_view(&pet).carry_count == 0);

    // Weekdays out (at work) aren't shopping trips either.
    pet = pet_at(TUE, 12, 0);
    pet_set_place(&pet, PET_PLACE_AWAY);
    tick(&pet, TUE, 12, 0, 1000);
    pet_set_place(&pet, PET_PLACE_HOME);
    tick(&pet, TUE, 14, 0, 1000 + 2 * PET_SHOP_TRIP_MS);
    CHECK(pet.bought_count == 0);
}

static void test_some_weekdays_he_brings_bags_home_from_work(void)
{
    int with = 0, without = 0;
    for (uint32_t seed = 1; seed <= 40; seed++) {
        pet_t pet;
        pet_init(&pet, seed);
        tick(&pet, TUE, 14, 0, 0);
        tick(&pet, TUE, 16, 0, 1000);  // on the train home
        CHECK(pet.bought_count == 0);
        tick(&pet, TUE, 17, 0, 2000);  // home: straight into the bath
        pet_view_t v = pet_view(&pet);
        CHECK(v.mood == PET_MOOD_ME_TIME && v.carry_count == 0 && v.bag_count == 0);
        int bought = pet.bought_count;
        bought ? with++ : without++;
        tick(&pet, TUE, 18, 0, 3000);  // only once a day
        CHECK(pet.bought_count == bought);
        // He brings them in when you can see the floor: after the bath.
        tick(&pet, TUE, 19, 30, 4000);
        CHECK((pet_view(&pet).carry_count > 0) == (bought > 0));
    }
    CHECK(with > 5 && without > 5);

    // Switched on in the evening: he didn't go to work as far as it knows.
    pet_t pet = pet_at(TUE, 20, 0);
    tick(&pet, TUE, 20, 5, 1000);
    CHECK(pet.bought_count == 0);
}

static void test_home_early_from_work(void)
{
    // Home Wi-Fi during the commute home: he's home, so the bags come now.
    int with = 0;
    for (uint32_t seed = 1; seed <= 20; seed++) {
        pet_t pet;
        pet_init(&pet, seed);
        tick(&pet, TUE, 15, 0, 0);
        pet_set_place(&pet, PET_PLACE_HOME);
        tick(&pet, TUE, 15, 40, 1000);
        CHECK(pet_view(&pet).mood == PET_MOOD_DEADPAN);
        with += pet_view(&pet).carry_count > 0;
    }
    CHECK(with > 0);
}

static void test_bags_wait_for_the_screen(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    pet_set_screen_on(&pet, false);
    pet.bought[0] = PET_BAG_SHOEBOX;
    pet.bought_count = 1;
    tick(&pet, SAT, 14, 0, 1000);
    CHECK(!pet.carrying);
    pet_set_screen_on(&pet, true);
    tick(&pet, SAT, 14, 0, 2000);
    pet_view_t v = pet_view(&pet);
    CHECK(v.carry_count == 1 && v.carry[0] == PET_BAG_SHOEBOX);
}

static void test_bags_hidden_out_of_the_house_but_kept(void)
{
    pet_t pet = pet_at(TUE, 12, 0);  // work
    put_bags(&pet, 3);
    CHECK(pet_view(&pet).bag_count == 0);
    pet_handle_event(&pet, clear_bag(0), 100);  // can't tap what isn't there
    CHECK(pet.bag_count == 3);
    tick(&pet, TUE, 18, 0, 200);  // bath
    CHECK(pet_view(&pet).bag_count == 0);
    tick(&pet, TUE, 23, 0, 300);  // bed
    CHECK(pet_view(&pet).bag_count == 0);
    tick(&pet, WED, 5, 30, 400);  // grumpy morning: there they are
    CHECK(pet_view(&pet).bag_count == 3);
    pet_handle_event(&pet, SWIPE, 500);  // spooky shows them too
    CHECK(pet_view(&pet).bag_count == 3);
}

static void test_tap_bags_away_for_a_heart(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    put_bags(&pet, 3);
    pet.bags[0] = PET_BAG_PAPER;
    pet.bags[1] = PET_BAG_TOTE;
    pet.bags[2] = PET_BAG_GUMMY;
    uint32_t pokes = pet.pokes;
    pet_handle_event(&pet, clear_bag(1), 100);
    pet_view_t v = pet_view(&pet);
    CHECK(v.bag_count == 2 && v.bags[0] == PET_BAG_PAPER && v.bags[1] == PET_BAG_GUMMY);  // shift down
    CHECK(v.line == NULL && v.happiness == PET_HAPPINESS_START);
    CHECK(pet.pokes == pokes);  // not a poke at him
    pet_handle_event(&pet, clear_bag(5), 200);  // nothing there
    CHECK(pet_view(&pet).bag_count == 2);
    pet_handle_event(&pet, clear_bag(0), 300);
    pet_handle_event(&pet, clear_bag(0), 400);
    v = pet_view(&pet);
    CHECK(v.bag_count == 0 && v.line != NULL && v.face == PET_FACE_SMIRK);
    CHECK(v.happiness == PET_HAPPINESS_START + 1);
}

static void test_too_many_bags_annoy_him(void)
{
    pet_t pet = pet_at(SAT, 14, 0);
    put_bags(&pet, PET_BAGS_MESSY);
    CHECK(pet_view(&pet).face == PET_FACE_BLANK);
    // Bringing more in fills the floor; he says so, and the rest don't fit.
    pet.bought[0] = pet.bought[1] = pet.bought[2] = PET_BAG_FASHION;
    pet.bought_count = 3;
    tick(&pet, SAT, 14, 0, 1000);
    CHECK(pet.carrying);
    tick(&pet, SAT, 14, 0, 1000 + CARRY_MS);
    pet_view_t v = pet_view(&pet);
    CHECK(v.bag_count == PET_BAGS_MAX && v.line != NULL && v.face == PET_FACE_ANNOYED);
    tick(&pet, SAT, 14, 1, 1000 + CARRY_MS + 5000);
    CHECK(pet_view(&pet).line == NULL && pet_view(&pet).face == PET_FACE_ANNOYED);  // still, at rest

    // And ignored, he gets unhappy twice as fast.
    uint32_t t0 = pet.last_care_ms;
    tick(&pet, SAT, 15, 0, t0 + PET_IGNORED_MS / 2);
    CHECK(pet_view(&pet).happiness == PET_HAPPINESS_START - 1);
}

static void test_same_seed_same_lines(void)
{
    pet_t a = pet_at(SAT, 14, 0), b = pet_at(SAT, 14, 0);
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

// --- Place (from Wi-Fi) ----------------------------------------------------------

static void test_commuting_puts_him_on_the_train(void)
{
    pet_t pet = pet_at(TUE, 10, 0);  // scheduled deadpan at work
    pet_set_place(&pet, PET_PLACE_COMMUTING);
    CHECK(tick(&pet, TUE, 10, 0, 1000));
    CHECK(pet_view(&pet).mood == PET_MOOD_COMMUTE);
    CHECK(pet_view(&pet).scene == PET_SCENE_NONE);

    // Over me-time too, and the scene goes.
    pet = pet_at(TUE, 18, 0);
    pet_set_place(&pet, PET_PLACE_COMMUTING);
    tick(&pet, TUE, 18, 0, 1000);
    CHECK(pet_view(&pet).mood == PET_MOOD_COMMUTE);
    CHECK(pet_view(&pet).scene == PET_SCENE_NONE);

    // He's on the train even on Halloween.
    pet_init(&pet, 1);
    pet_set_place(&pet, PET_PLACE_COMMUTING);
    pet_clock_t halloween = {.month = 10, .mday = 31, .wday = SAT, .hour = 14};
    pet_tick(&pet, &halloween, 0);
    CHECK(pet_view(&pet).mood == PET_MOOD_COMMUTE);
}

static void test_commuting_never_wakes_him(void)
{
    pet_t pet = pet_at(TUE, 3, 0);
    pet_set_place(&pet, PET_PLACE_COMMUTING);
    tick(&pet, TUE, 3, 0, 1000);
    CHECK(pet_view(&pet).mood == PET_MOOD_ASLEEP);
}

static void test_home_cancels_scheduled_commute(void)
{
    pet_t pet = pet_at(TUE, 6, 30);
    CHECK(pet_view(&pet).mood == PET_MOOD_COMMUTE);
    pet_set_place(&pet, PET_PLACE_HOME);
    CHECK(tick(&pet, TUE, 6, 30, 1000));
    CHECK(pet_view(&pet).mood == PET_MOOD_DEADPAN);

    // Home leaves the rest of the schedule alone.
    pet = pet_at(TUE, 18, 0);
    pet_set_place(&pet, PET_PLACE_HOME);
    tick(&pet, TUE, 18, 0, 1000);
    CHECK(pet_view(&pet).mood == PET_MOOD_ME_TIME);
    CHECK(pet_view(&pet).scene == PET_SCENE_BATH);
}

static void test_unknown_place_follows_the_schedule(void)
{
    pet_t pet = pet_at(TUE, 10, 0);
    pet_set_place(&pet, PET_PLACE_COMMUTING);
    tick(&pet, TUE, 10, 0, 1000);
    pet_set_place(&pet, PET_PLACE_UNKNOWN);
    CHECK(tick(&pet, TUE, 10, 0, 2000));
    CHECK(pet_view(&pet).mood == PET_MOOD_WORK);
}

static void test_costume_still_beats_the_train(void)
{
    pet_t pet = pet_at(TUE, 10, 0);
    pet_set_place(&pet, PET_PLACE_COMMUTING);
    tick(&pet, TUE, 10, 0, 1000);
    pet_handle_event(&pet, SWIPE, 2000);
    CHECK(pet_view(&pet).mood == PET_MOOD_SPOOKY);
}

// --- Charging ---------------------------------------------------------------------

static void test_charging_means_me_time(void)
{
    pet_t pet = pet_at(TUE, 10, 0);  // scheduled deadpan at work
    pet_set_charging(&pet, true);
    CHECK(tick(&pet, TUE, 10, 0, 1000));
    CHECK(pet_view(&pet).mood == PET_MOOD_ME_TIME);
    CHECK(pet_view(&pet).scene == PET_SCENE_BATH);  // weekday: bath

    pet = pet_at(SAT, 14, 0);
    pet_set_charging(&pet, true);
    tick(&pet, SAT, 14, 0, 1000);
    CHECK(pet_view(&pet).mood == PET_MOOD_ME_TIME);
    CHECK(pet_view(&pet).scene == PET_SCENE_COUCH);  // weekend, not summer

    // Off the charger, back to the schedule.
    pet_set_charging(&pet, false);
    CHECK(tick(&pet, SAT, 14, 0, 2000));
    CHECK(pet_view(&pet).mood != PET_MOOD_ME_TIME);
}

static void test_charging_beats_the_train_but_not_bed(void)
{
    pet_t pet = pet_at(TUE, 10, 0);
    pet_set_place(&pet, PET_PLACE_COMMUTING);
    pet_set_charging(&pet, true);
    tick(&pet, TUE, 10, 0, 1000);
    CHECK(pet_view(&pet).mood == PET_MOOD_ME_TIME);

    pet = pet_at(TUE, 3, 0);
    pet_set_charging(&pet, true);
    tick(&pet, TUE, 3, 0, 1000);
    CHECK(pet_view(&pet).mood == PET_MOOD_ASLEEP);
}

static void test_costume_still_beats_the_charger(void)
{
    pet_t pet = pet_at(TUE, 10, 0);
    pet_set_charging(&pet, true);
    tick(&pet, TUE, 10, 0, 1000);
    pet_handle_event(&pet, SWIPE, 2000);
    CHECK(pet_view(&pet).mood == PET_MOOD_SPOOKY);
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
    test_at_work();
    test_tap_smirks_when_deadpan();
    test_tap_annoys_when_grumpy_and_costs_a_heart();
    test_fumbled_double_tap_in_the_bath_is_cheap();
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
    test_pokes_in_the_sulk_bath_add_up_but_never_past_a_full_wait();
    test_no_sulk_from_sleepy_pokes();
    test_swipe_dresses_him_up_any_time();
    test_swiping_through_every_costume();
    test_costume_stays_on_through_the_day_and_off_for_bed();
    test_scheduled_spooky_just_wraps();
    test_no_dressing_up_while_asleep();
    test_swipe_up_cycles_scenes_then_back_to_schedule();
    test_swipe_up_in_scheduled_me_time_moves_on();
    test_swipe_up_reaches_the_bath_on_a_lazy_sunday();
    test_swipe_up_takes_the_costume_off();
    test_chosen_scene_lasts_until_bed();
    test_no_scene_swipe_while_asleep_or_sulking();
    test_sulking_on_the_couch_gets_him_up();
    test_weekend_shopping_trip_brings_bags_home();
    test_a_quick_pop_out_is_not_shopping();
    test_some_weekdays_he_brings_bags_home_from_work();
    test_home_early_from_work();
    test_bags_wait_for_the_screen();
    test_bags_hidden_out_of_the_house_but_kept();
    test_tap_bags_away_for_a_heart();
    test_too_many_bags_annoy_him();
    test_same_seed_same_lines();
    test_names();
    test_commuting_puts_him_on_the_train();
    test_commuting_never_wakes_him();
    test_home_cancels_scheduled_commute();
    test_unknown_place_follows_the_schedule();
    test_costume_still_beats_the_train();
    test_charging_means_me_time();
    test_charging_beats_the_train_but_not_bed();
    test_costume_still_beats_the_charger();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all pet tests passed\n");
    return EXIT_SUCCESS;
}

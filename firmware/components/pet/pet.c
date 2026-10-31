#include "pet.h"

#include <stddef.h>

// --- Schedule ----------------------------------------------------------------
// Weekdays: up at 5, commute, work, commute, me-time, bed by 9. Friday night
// is horror night. Weekends: lie-in until almost noon, lazy Sunday, late
// Saturday. All of 31 October is spooky while he's awake.

static bool is_weekend(const pet_clock_t *c)
{
    return c->wday == 0 || c->wday == 6;
}

pet_mood_t pet_mood_for(const pet_clock_t *c)
{
    int m = c->hour * 60 + c->minute;  // minutes since midnight
#define AT(h, min) ((h) * 60 + (min))
    pet_mood_t mood;
    if (!is_weekend(c)) {
        if (c->wday == 5 && m >= AT(19, 0)) mood = PET_MOOD_SPOOKY;
        else if (m < AT(5, 0) || m >= AT(21, 0)) mood = PET_MOOD_ASLEEP;
        else if (m < AT(6, 0)) mood = PET_MOOD_GRUMPY;
        else if (m < AT(7, 0)) mood = PET_MOOD_COMMUTE;
        else if (m < AT(15, 30)) mood = PET_MOOD_DEADPAN;
        else if (m < AT(17, 0)) mood = PET_MOOD_COMMUTE;
        else if (m < AT(19, 30)) mood = PET_MOOD_ME_TIME;
        else mood = PET_MOOD_NIGHT;
    } else {
        bool sunday = c->wday == 0;
        if (m < AT(2, 0)) mood = sunday ? PET_MOOD_SPOOKY : PET_MOOD_NIGHT;  // Fri/Sat night running late
        else if (m < AT(11, 30)) mood = PET_MOOD_ASLEEP;
        else if (m < AT(13, 0)) mood = PET_MOOD_GRUMPY;
        else if (m < AT(19, 0)) mood = sunday ? PET_MOOD_ME_TIME : PET_MOOD_DEADPAN;
        else if (!sunday) mood = PET_MOOD_NIGHT;
        else if (m < AT(21, 0)) mood = PET_MOOD_ME_TIME;
        else if (m < AT(22, 0)) mood = PET_MOOD_NIGHT;
        else mood = PET_MOOD_ASLEEP;
    }
#undef AT
    if (c->month == 10 && c->mday == 31 && mood != PET_MOOD_ASLEEP) {
        mood = PET_MOOD_SPOOKY;
    }
    return mood;
}

// Where he'd have me-time on this day: bath on weekdays, couch or beach at
// the weekend.
static pet_scene_t me_time_scene(const pet_clock_t *c)
{
    if (!is_weekend(c)) {
        return PET_SCENE_BATH;
    }
    bool summer = c->month >= 6 && c->month <= 8;
    return summer ? PET_SCENE_BEACH : PET_SCENE_COUCH;
}

pet_scene_t pet_scene_for(const pet_clock_t *c)
{
    return pet_mood_for(c) == PET_MOOD_ME_TIME ? me_time_scene(c) : PET_SCENE_NONE;
}

// --- Names -------------------------------------------------------------------

bool pet_costume_is_cute(pet_costume_t c)
{
    return c == PET_COSTUME_CHIIKAWA || c == PET_COSTUME_PIKACHU || c == PET_COSTUME_SYLVEON ||
           c == PET_COSTUME_TREX || c == PET_COSTUME_TRIKE;
}

const char *pet_mood_name(pet_mood_t mood)
{
    static const char *const names[PET_MOOD_COUNT] = {
        [PET_MOOD_ASLEEP] = "asleep",   [PET_MOOD_GRUMPY] = "grumpy", [PET_MOOD_COMMUTE] = "commute",
        [PET_MOOD_DEADPAN] = "deadpan", [PET_MOOD_ME_TIME] = "me-time", [PET_MOOD_NIGHT] = "night",
        [PET_MOOD_SPOOKY] = "spooky",
    };
    if ((int)mood < 0 || mood >= PET_MOOD_COUNT) {
        return "unknown";
    }
    return names[mood];
}

const char *pet_costume_name(pet_costume_t c)
{
    static const char *const names[PET_COSTUME_COUNT] = {
        [PET_COSTUME_DRACULA] = "dracula", [PET_COSTUME_CHIIKAWA] = "chiikawa", [PET_COSTUME_SADAKO] = "sadako",
        [PET_COSTUME_TREX] = "trex",       [PET_COSTUME_BARREL] = "barrel",     [PET_COSTUME_PIKACHU] = "pikachu",
        [PET_COSTUME_JASON] = "jason",     [PET_COSTUME_TRIKE] = "trike",       [PET_COSTUME_SYLVEON] = "sylveon",
    };
    if ((int)c < 0 || c >= PET_COSTUME_COUNT) {
        return "unknown";
    }
    return names[c];
}

// --- Lines -------------------------------------------------------------------
// What he says, from the mockup. Each list ends with NULL.

typedef const char *const lines_t[];

static lines_t L_GRUMPY_TAP = {"…no.", "ねむい", "not yet.", NULL};
static lines_t L_GRUMPY_SHAKE = {"too early.", "やめて…", NULL};
static lines_t L_DEADPAN_TAP = {"heh.", "ふーん", "…ok.", NULL};
static lines_t L_DEADPAN_SHAKE = {"…why.", "やめて", NULL};
static lines_t L_BATH_TAP = {"me time.", "ひとり時間なの", NULL};
static lines_t L_BATH_SHAKE = {"oi.", "ちょっと!", NULL};
static lines_t L_COUCH_TAP = {"いいところなの", "one more chapter.", NULL};
static lines_t L_COUCH_SHAKE = {"oi. lost my page.", NULL};
static lines_t L_BEACH_TAP = {"…shh. waves.", "ひとり時間", NULL};
static lines_t L_BEACH_SHAKE = {"oi. sand.", "こぼれる!", NULL};
static lines_t L_SPOOKY_SHAKE = {"…why.", "こわっ", NULL};
static lines_t L_NIGHT_SHAKE = {"oi.", "こら", NULL};
static lines_t L_ASLEEP_TAP = {"…ん?", "zzz…", NULL};
static lines_t L_ASLEEP_SHAKE = {"うるさい…", "…what time is it", NULL};
static lines_t L_COMMUTE_TAP = {"…(headphones on)", "まだ?", "mind the gap.", NULL};
static lines_t L_COMMUTE_SHAKE = {"ugh. Central line.", "oi. personal space.", NULL};
static lines_t L_PHONE = {"one more reel.", "あと一本", NULL};
static lines_t L_SWITCH = {"shh. boss fight.", "ちょっと待って", NULL};
static lines_t L_BACK = {"…what.", "なに?", NULL};
static lines_t L_BATH_BACK = {"見ないで!", "oi. eyes up.", "えっち", NULL};
static lines_t L_NO_TURN_ASLEEP = {"zzz…", NULL};
static lines_t L_NO_TURN_COMFY = {"comfy. no.", NULL};
static lines_t L_GUMMY = {"うまっ", "more.", "グミ…♡", NULL};
static lines_t L_CRISPS = {"one more.", "パリパリ", "salty. good.", NULL};
static lines_t L_COFFEE = {"ありがと", "iced. good.", NULL};
static lines_t L_COFFEE_MORNING = {"ok. alive.", "…おはよ", NULL};
static lines_t L_COFFEE_NIGHT = {"coffee at night? …ok.", "夜なのに", NULL};
static lines_t L_COFFEE_BEACH = {"piña colada pls.", NULL};
static lines_t L_SNACK_ASLEEP = {"…むにゃ", NULL};
// Sulking (not in the mockup yet).
static lines_t L_SULK_START = {"…fine.", "もういい", NULL};
static lines_t L_SULK = {"…", "ふん", "not talking.", NULL};
static lines_t L_SULK_BATH = {"bath. alone.", "ひとりにして", NULL};
static lines_t L_SULK_BATH_POKED = {"oi. bath.", "まだ!", NULL};
static lines_t L_FORGIVE = {"…ok. hi.", "まあいいか", NULL};

static const char *const *costume_lines(pet_costume_t c)
{
    static lines_t dracula = {"bleh.", "ブラー", NULL};
    static lines_t sadako = {"…来る", "…", NULL};
    static lines_t jason = {"ch ch ch", "ki ki ki", NULL};
    static lines_t pikachu = {"pika.", "ピカ…", NULL};
    static lines_t sylveon = {"ribbons.", "ニンフィア♡", NULL};
    static lines_t chiikawa = {"ワッ", "ヤダ…", "フ…", NULL};
    static lines_t trex = {"rawr.", "ガオー", "…rawr. (flat)", NULL};
    static lines_t trike = {"three horns. leave me.", "ガオ…", NULL};
    static lines_t barrel = {"trick or treat.", "ヒヒッ", "boo. (bored)", NULL};
    switch (c) {
    case PET_COSTUME_DRACULA: return dracula;
    case PET_COSTUME_SADAKO: return sadako;
    case PET_COSTUME_JASON: return jason;
    case PET_COSTUME_PIKACHU: return pikachu;
    case PET_COSTUME_SYLVEON: return sylveon;
    case PET_COSTUME_CHIIKAWA: return chiikawa;
    case PET_COSTUME_TREX: return trex;
    case PET_COSTUME_TRIKE: return trike;
    default: return barrel;
    }
}

static uint32_t next_rand(pet_t *pet)
{
    // xorshift32
    uint32_t x = pet->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    pet->rng = x;
    return x;
}

static const char *pick(pet_t *pet, const char *const *lines)
{
    size_t n = 0;
    while (lines[n] != NULL) {
        n++;
    }
    return lines[next_rand(pet) % n];
}

// --- State helpers -----------------------------------------------------------

// The mockup runs at ~110 ms a frame; reaction lengths are in those frames.
#define FRAME_MS 110u

// Wrap-safe "a is at or after b" for a uint32 millisecond clock.
static bool time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

void pet_init(pet_t *pet, uint32_t seed)
{
    *pet = (pet_t){
        .scheduled = PET_MOOD_DEADPAN,
        .happiness = PET_HAPPINESS_START,
        .costume = PET_COSTUME_DRACULA,
        .device = PET_DEVICE_PHONE,
        .rng = seed ? seed : 0x7011u,
    };
}

// The mood he is actually in. A sulk bath, then a costume, override the
// schedule, but nothing overrides sleep.
static pet_mood_t current_mood(const pet_t *pet)
{
    if (pet->scheduled == PET_MOOD_ASLEEP) {
        return PET_MOOD_ASLEEP;
    }
    if (pet->sulk_bathing) {
        return PET_MOOD_ME_TIME;
    }
    if (pet->dressed_up) {
        return PET_MOOD_SPOOKY;
    }
    if (pet->chosen_scene != PET_SCENE_NONE) {
        return PET_MOOD_ME_TIME;
    }
    return pet->scheduled;
}

static pet_scene_t current_scene(const pet_t *pet)
{
    if (current_mood(pet) != PET_MOOD_ME_TIME) {
        return PET_SCENE_NONE;
    }
    if (pet->sulk_bathing) {
        return PET_SCENE_BATH;
    }
    if (pet->chosen_scene != PET_SCENE_NONE) {
        return pet->chosen_scene;
    }
    return pet->scene;
}

// He can only turn around when he's up: not asleep, not lying down.
static bool can_turn(const pet_t *pet)
{
    pet_mood_t mood = current_mood(pet);
    if (mood == PET_MOOD_ASLEEP) {
        return false;
    }
    return mood != PET_MOOD_ME_TIME || current_scene(pet) == PET_SCENE_BATH;
}

static void say(pet_t *pet, const char *line, pet_face_t face, unsigned frames, uint32_t now_ms)
{
    pet->reacting = true;
    pet->react_line = line;
    pet->react_face = face;
    pet->react_item = PET_ITEM_NONE;
    pet->react_shake = false;
    pet->react_until_ms = now_ms + frames * FRAME_MS;
}

static void say_costume(pet_t *pet, uint32_t now_ms)
{
    pet_face_t face = pet_costume_is_cute(pet->costume) ? PET_FACE_SMIRK : PET_FACE_FANG;
    say(pet, pick(pet, costume_lines(pet->costume)), face, 12, now_ms);
}

static void start_sulk(pet_t *pet, uint32_t now_ms)
{
    pet->sulking = true;
    pet->sulk_bathing = false;
    pet->chosen_scene = PET_SCENE_NONE;  // gets up so he can turn his back
    pet->back = false;
    pet->menu_open = false;
    say(pet, pick(pet, L_SULK_START), PET_FACE_ANNOYED, 16, now_ms);
}

// Change happiness, clamped. Hitting zero while awake starts a sulk.
static void add_happiness(pet_t *pet, int delta, uint32_t now_ms)
{
    int h = pet->happiness + delta;
    if (h < 0) h = 0;
    if (h > PET_HAPPINESS_MAX) h = PET_HAPPINESS_MAX;
    pet->happiness = h;
    if (h == 0 && !pet->sulking && current_mood(pet) != PET_MOOD_ASLEEP) {
        start_sulk(pet, now_ms);
    }
}

// --- Time ----------------------------------------------------------------------

void pet_set_place(pet_t *pet, pet_place_t place)
{
    pet->place = place < PET_PLACE_COUNT ? place : PET_PLACE_UNKNOWN;
}

void pet_set_charging(pet_t *pet, bool charging)
{
    pet->charging = charging;
}

bool pet_tick(pet_t *pet, const pet_clock_t *clock, uint32_t now_ms)
{
    bool changed = false;

    pet_mood_t mood = pet_mood_for(clock);
    pet_scene_t scene = pet_scene_for(clock);
    if (mood != PET_MOOD_ASLEEP) {
        if (pet->place == PET_PLACE_COMMUTING) {
            mood = PET_MOOD_COMMUTE;
        } else if (pet->place == PET_PLACE_HOME && mood == PET_MOOD_COMMUTE) {
            mood = PET_MOOD_DEADPAN;
        }
        if (pet->charging) {  // on the charger: me-time, wherever he is
            mood = PET_MOOD_ME_TIME;
            scene = me_time_scene(clock);
        }
        if (mood != PET_MOOD_ME_TIME) {
            scene = PET_SCENE_NONE;
        }
    }
    if (mood != pet->scheduled || scene != pet->scene) {
        pet->scheduled = mood;
        pet->scene = scene;
        pet->back = false;
        pet->reacting = false;
        if (mood == PET_MOOD_ASLEEP) {
            pet->dressed_up = false;  // costume off for bed
            pet->chosen_scene = PET_SCENE_NONE;
        }
        changed = true;
    }

    if (pet->reacting && time_reached(now_ms, pet->react_until_ms)) {
        pet->reacting = false;
        changed = true;
    }

    if (pet->menu_open && time_reached(now_ms, pet->menu_until_ms)) {
        pet->menu_open = false;
        changed = true;
    }

    // Left alone too long, he gets less happy. Not while asleep or sulking.
    if (current_mood(pet) == PET_MOOD_ASLEEP || pet->sulking) {
        pet->last_care_ms = now_ms;
    } else if (now_ms - pet->last_care_ms >= PET_IGNORED_MS) {
        pet->last_care_ms = now_ms;
        add_happiness(pet, -1, now_ms);
        changed = true;
    }

    // Enough undisturbed bath time and he forgives you.
    if (pet->sulk_bathing && now_ms - pet->sulk_bath_since_ms >= PET_SULK_BATH_MS) {
        pet->sulking = false;
        pet->sulk_bathing = false;
        say(pet, pick(pet, L_FORGIVE), PET_FACE_SMIRK, 18, now_ms);
        changed = true;
    }

    return changed;
}

// --- Input ---------------------------------------------------------------------

static const char *const *tap_lines(pet_mood_t mood, pet_scene_t scene)
{
    switch (mood) {
    case PET_MOOD_GRUMPY: return L_GRUMPY_TAP;
    case PET_MOOD_COMMUTE: return L_COMMUTE_TAP;
    case PET_MOOD_ME_TIME:
        return scene == PET_SCENE_COUCH ? L_COUCH_TAP : scene == PET_SCENE_BEACH ? L_BEACH_TAP : L_BATH_TAP;
    case PET_MOOD_ASLEEP: return L_ASLEEP_TAP;
    default: return L_DEADPAN_TAP;
    }
}

static const char *const *shake_lines(pet_mood_t mood, pet_scene_t scene)
{
    switch (mood) {
    case PET_MOOD_GRUMPY: return L_GRUMPY_SHAKE;
    case PET_MOOD_COMMUTE: return L_COMMUTE_SHAKE;
    case PET_MOOD_ME_TIME:
        return scene == PET_SCENE_COUCH ? L_COUCH_SHAKE : scene == PET_SCENE_BEACH ? L_BEACH_SHAKE : L_BATH_SHAKE;
    case PET_MOOD_ASLEEP: return L_ASLEEP_SHAKE;
    case PET_MOOD_SPOOKY: return L_SPOOKY_SHAKE;
    case PET_MOOD_NIGHT: return L_NIGHT_SHAKE;
    default: return L_DEADPAN_SHAKE;
    }
}

static void on_tap(pet_t *pet, uint32_t now_ms)
{
    pet_mood_t mood = current_mood(pet);
    pet_scene_t scene = current_scene(pet);
    pet->pokes++;

    if (pet->menu_open) {  // tapping away closes the snack picker
        pet->menu_open = false;
        return;
    }
    if (mood == PET_MOOD_ASLEEP) {
        say(pet, pick(pet, L_ASLEEP_TAP), PET_FACE_SLEEP, 16, now_ms);
        add_happiness(pet, -1, now_ms);
        return;
    }
    if (pet->sulk_bathing) {  // disturbed: the clock starts again
        pet->sulk_bath_since_ms = now_ms;
        say(pet, pick(pet, L_SULK_BATH_POKED), PET_FACE_ANNOYED, 14, now_ms);
        return;
    }
    if (pet->sulking) {
        say(pet, pick(pet, L_SULK), PET_FACE_ANNOYED, 12, now_ms);
        return;
    }
    if (pet->back && can_turn(pet)) {
        pet->back = false;
        if (mood == PET_MOOD_ME_TIME) {
            say(pet, pick(pet, L_BATH_BACK), PET_FACE_ANNOYED, 14, now_ms);
        } else {
            say(pet, pick(pet, L_BACK), mood == PET_MOOD_NIGHT ? PET_FACE_SCREEN : PET_FACE_BLANK, 12, now_ms);
        }
        return;
    }
    if (mood == PET_MOOD_SPOOKY) {  // each tap: next costume
        pet->costume = (pet->costume + 1) % PET_COSTUME_COUNT;
        say_costume(pet, now_ms);
        return;
    }
    if (mood == PET_MOOD_NIGHT) {  // each tap: swap phone and Switch
        pet->device = pet->device == PET_DEVICE_PHONE ? PET_DEVICE_SWITCH : PET_DEVICE_PHONE;
        say(pet, pick(pet, pet->device == PET_DEVICE_PHONE ? L_PHONE : L_SWITCH), PET_FACE_SCREEN, 14, now_ms);
        return;
    }
    pet_face_t face = mood == PET_MOOD_DEADPAN ? PET_FACE_SMIRK : PET_FACE_ANNOYED;
    say(pet, pick(pet, tap_lines(mood, scene)), face, 16, now_ms);
    if (mood == PET_MOOD_ME_TIME || mood == PET_MOOD_GRUMPY) {
        add_happiness(pet, -1, now_ms);
    }
}

static void on_shake(pet_t *pet, uint32_t now_ms)
{
    pet_mood_t mood = current_mood(pet);
    pet_face_t face;
    switch (mood) {
    case PET_MOOD_ASLEEP: face = PET_FACE_SLEEP; break;
    case PET_MOOD_DEADPAN:
    case PET_MOOD_SPOOKY: face = PET_FACE_DIZZY; break;
    default: face = PET_FACE_ANNOYED; break;
    }
    if (pet->sulk_bathing) {
        pet->sulk_bath_since_ms = now_ms;
    }
    say(pet, pick(pet, shake_lines(mood, current_scene(pet))), face, 14, now_ms);
    pet->react_shake = true;
    add_happiness(pet, -1, now_ms);
}

static void on_turn(pet_t *pet, uint32_t now_ms)
{
    if (pet->sulking && !pet->sulk_bathing) {
        say(pet, pick(pet, L_SULK), PET_FACE_ANNOYED, 12, now_ms);
        return;
    }
    if (!can_turn(pet) || pet->sulk_bathing) {
        bool asleep = current_mood(pet) == PET_MOOD_ASLEEP;
        say(pet, pick(pet, asleep ? L_NO_TURN_ASLEEP : L_NO_TURN_COMFY), asleep ? PET_FACE_SLEEP : PET_FACE_ANNOYED,
            10, now_ms);
        return;
    }
    pet->back = !pet->back;
    pet->reacting = false;
}

static void on_give(pet_t *pet, pet_item_t item, uint32_t now_ms)
{
    pet_mood_t mood = current_mood(pet);
    pet_scene_t scene = current_scene(pet);
    pet->menu_open = false;
    pet->back = false;

    if (item <= PET_ITEM_NONE || item >= PET_ITEM_COUNT) {
        return;
    }
    if (mood == PET_MOOD_ASLEEP) {
        say(pet, pick(pet, L_SNACK_ASLEEP), PET_FACE_SLEEP, 14, now_ms);
        add_happiness(pet, 1, now_ms);
        return;
    }

    const char *const *lines = item == PET_ITEM_GUMMY ? L_GUMMY : item == PET_ITEM_CRISPS ? L_CRISPS : L_COFFEE;
    pet_face_t face = PET_FACE_SMIRK;
    if (item == PET_ITEM_COFFEE && mood == PET_MOOD_GRUMPY) lines = L_COFFEE_MORNING;
    if (item == PET_ITEM_COFFEE && mood == PET_MOOD_NIGHT) lines = L_COFFEE_NIGHT;
    if (item == PET_ITEM_COFFEE && scene == PET_SCENE_BEACH) lines = L_COFFEE_BEACH;
    if (mood == PET_MOOD_ME_TIME) face = PET_FACE_HAPPY;
    if (mood == PET_MOOD_SPOOKY && !pet_costume_is_cute(pet->costume)) face = PET_FACE_FANG;

    // Snacks and coffee always help. Coffee helps most first thing.
    add_happiness(pet, item == PET_ITEM_COFFEE && mood == PET_MOOD_GRUMPY ? 2 : 1, now_ms);

    if (pet->sulking && !pet->sulk_bathing) {
        if (pet->happiness >= PET_SULK_FED) {  // fed enough: off to the bath
            pet->sulk_bathing = true;
            pet->sulk_bath_since_ms = now_ms;
            say(pet, pick(pet, L_SULK_BATH), PET_FACE_BLANK, 18, now_ms);
        } else {  // eats it, still facing away
            say(pet, pick(pet, L_SULK), PET_FACE_BLANK, 18, now_ms);
            pet->react_item = item;
        }
        return;
    }
    say(pet, pick(pet, lines), face, 18, now_ms);
    pet->react_item = item;
}

static void on_swipe(pet_t *pet, int direction, uint32_t now_ms)
{
    pet_mood_t mood = current_mood(pet);
    if (mood == PET_MOOD_ASLEEP) {
        say(pet, pick(pet, L_NO_TURN_ASLEEP), PET_FACE_SLEEP, 10, now_ms);
        return;
    }
    if (pet->sulking) {
        say(pet, pick(pet, L_SULK), PET_FACE_ANNOYED, 12, now_ms);
        return;
    }
    pet->back = false;
    if (mood != PET_MOOD_SPOOKY) {  // put on the current costume
        pet->dressed_up = true;
        say_costume(pet, now_ms);
        return;
    }
    int next = (int)pet->costume + (direction < 0 ? -1 : 1);
    bool past_end = next < 0 || next >= PET_COSTUME_COUNT;
    if (past_end && pet->dressed_up && pet->scheduled != PET_MOOD_SPOOKY) {
        // Back to normal clothes. The next swipe starts at the other end.
        pet->dressed_up = false;
        pet->costume = next < 0 ? PET_COSTUME_COUNT - 1 : 0;
        pet->reacting = false;
        return;
    }
    pet->costume = (pet_costume_t)((next + PET_COSTUME_COUNT) % PET_COSTUME_COUNT);
    say_costume(pet, now_ms);
}

static void on_swipe_up(pet_t *pet, uint32_t now_ms)
{
    pet_mood_t mood = current_mood(pet);
    if (mood == PET_MOOD_ASLEEP) {
        say(pet, pick(pet, L_NO_TURN_ASLEEP), PET_FACE_SLEEP, 10, now_ms);
        return;
    }
    if (pet->sulking) {
        say(pet, pick(pet, L_SULK), PET_FACE_ANNOYED, 12, now_ms);
        return;
    }
    // Step on from the scene he's in now: bath, couch, beach, then schedule.
    pet_scene_t next = (pet_scene_t)(current_scene(pet) + 1);
    pet->chosen_scene = next < PET_SCENE_COUNT ? next : PET_SCENE_NONE;
    pet->dressed_up = false;
    pet->back = false;
    pet->reacting = false;
}

bool pet_handle_event(pet_t *pet, pet_event_t event, uint32_t now_ms)
{
    pet->last_care_ms = now_ms;
    switch (event.type) {
    case PET_INPUT_TAP: on_tap(pet, now_ms); return true;
    case PET_INPUT_SHAKE: on_shake(pet, now_ms); return true;
    case PET_INPUT_DOUBLE_TAP: on_turn(pet, now_ms); return true;
    case PET_INPUT_LONG_PRESS:
        pet->menu_open = true;
        pet->menu_until_ms = now_ms + PET_MENU_MS;
        return true;
    case PET_INPUT_GIVE: on_give(pet, event.item, now_ms); return true;
    case PET_INPUT_SWIPE: on_swipe(pet, event.direction, now_ms); return true;
    case PET_INPUT_SWIPE_UP: on_swipe_up(pet, now_ms); return true;
    }
    return false;
}

// --- View ----------------------------------------------------------------------

pet_view_t pet_view(const pet_t *pet)
{
    pet_view_t v = {
        .mood = current_mood(pet),
        .scene = current_scene(pet),
        .outfit = PET_OUTFIT_CASUAL,
        .costume = pet->costume,
        .face = PET_FACE_BLANK,
        .menu_open = pet->menu_open,
        .sulking = pet->sulking,
        .happiness = pet->happiness,
    };
    pet_item_t item = pet->reacting ? pet->react_item : PET_ITEM_NONE;

    switch (v.mood) {
    case PET_MOOD_ASLEEP:
        v.outfit = PET_OUTFIT_HIDDEN;
        v.face = PET_FACE_SLEEP;
        break;
    case PET_MOOD_GRUMPY:
        v.face = PET_FACE_GRUMPY;
        v.cup = item == PET_ITEM_NONE;
        v.bedhead = true;
        break;
    case PET_MOOD_COMMUTE:
        v.outfit = PET_OUTFIT_COAT;
        v.phone = item == PET_ITEM_NONE;
        break;
    case PET_MOOD_DEADPAN:
        v.phone = item == PET_ITEM_NONE;
        break;
    case PET_MOOD_ME_TIME:
        if (v.scene == PET_SCENE_BATH) {
            v.outfit = PET_OUTFIT_HIDDEN;
            v.face = PET_FACE_HAPPY;
        } else {
            v.outfit = PET_OUTFIT_LYING;
            v.face = v.scene == PET_SCENE_COUCH ? PET_FACE_READ : PET_FACE_SHADES;
        }
        break;
    case PET_MOOD_NIGHT:
        v.outfit = PET_OUTFIT_PJS;
        v.face = PET_FACE_SCREEN;
        v.device = item == PET_ITEM_NONE ? pet->device : PET_DEVICE_NONE;
        break;
    case PET_MOOD_SPOOKY:
        v.outfit = PET_OUTFIT_COSTUME;
        break;
    default:
        break;
    }

    // Facing away: turned by a double-tap, or sulking (not in his sulk bath).
    v.back = can_turn(pet) && (pet->back || (pet->sulking && !pet->sulk_bathing));
    if (v.back && v.mood == PET_MOOD_ME_TIME) {
        v.outfit = PET_OUTFIT_BATH;  // stands up out of the tub
    }

    if (pet->reacting) {
        v.face = pet->react_face;
        v.item = item;
        v.shake = pet->react_shake;
        v.line = pet->react_line;
    }
    return v;
}

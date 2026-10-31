// Pet logic: Tomo's state and how it reacts to the world.
//
// Pure C with no ESP-IDF or hardware calls, so it builds and is unit-tested
// on a normal computer (see tests/host). The firmware feeds it events and the
// time; the screen code reads the state to decide what to draw.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PET_MOOD_DEADPAN = 0,  // default: scrolling reels, rare wry smirk
    PET_MOOD_GRUMPY,       // mornings, before coffee
    PET_MOOD_SPOOKY,       // dressed up in a horror costume
    PET_MOOD_ME_TIME,      // bath + iced coffee, dislikes being poked
    PET_MOOD_COUNT,
} pet_mood_t;

typedef enum {
    PET_EVENT_POKE,   // screen tapped
    PET_EVENT_SHAKE,  // device shaken
} pet_event_t;

// A short-lived reaction shown on top of the mood, e.g. after a poke.
typedef enum {
    PET_REACTION_NONE = 0,
    PET_REACTION_SMIRK,
    PET_REACTION_ANNOYED,
    PET_REACTION_DIZZY,
} pet_reaction_t;

typedef struct {
    pet_mood_t mood;
    pet_reaction_t reaction;
    uint32_t reaction_until_ms;  // reaction clears once now_ms passes this
    uint32_t pokes;              // lifetime poke count, for fun later
} pet_t;

// How long a reaction stays on screen.
#define PET_REACTION_MS 1500u

void pet_init(pet_t *pet);

// Advance time. `hour` is local hour 0-23; `now_ms` is a monotonic clock.
// Returns true if anything visible changed and the screen should redraw.
bool pet_tick(pet_t *pet, int hour, uint32_t now_ms);

// React to an input. Returns true if the screen should redraw.
bool pet_handle_event(pet_t *pet, pet_event_t event, uint32_t now_ms);

// Base mood for a given hour, before any events. Exposed for tests.
pet_mood_t pet_mood_for_hour(int hour);

const char *pet_mood_name(pet_mood_t mood);

#ifdef __cplusplus
}
#endif

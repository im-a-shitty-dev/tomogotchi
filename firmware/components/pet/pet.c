#include "pet.h"

#include <stddef.h>

// Placeholder schedule until the character design settles. Only the morning
// grump is decided so far; everything else is deadpan.
pet_mood_t pet_mood_for_hour(int hour)
{
    if (hour >= 6 && hour < 10) {
        return PET_MOOD_GRUMPY;
    }
    return PET_MOOD_DEADPAN;
}

const char *pet_mood_name(pet_mood_t mood)
{
    static const char *const names[PET_MOOD_COUNT] = {
        [PET_MOOD_DEADPAN] = "deadpan",
        [PET_MOOD_GRUMPY] = "grumpy",
        [PET_MOOD_SPOOKY] = "spooky",
        [PET_MOOD_ME_TIME] = "me-time",
    };
    if (mood < 0 || mood >= PET_MOOD_COUNT) {
        return "unknown";
    }
    return names[mood];
}

void pet_init(pet_t *pet)
{
    *pet = (pet_t){
        .mood = PET_MOOD_DEADPAN,
        .reaction = PET_REACTION_NONE,
    };
}

// Wrap-safe "a is at or after b" for a uint32 millisecond clock.
static bool time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

bool pet_tick(pet_t *pet, int hour, uint32_t now_ms)
{
    bool changed = false;

    pet_mood_t mood = pet_mood_for_hour(hour);
    if (mood != pet->mood) {
        pet->mood = mood;
        changed = true;
    }

    if (pet->reaction != PET_REACTION_NONE && time_reached(now_ms, pet->reaction_until_ms)) {
        pet->reaction = PET_REACTION_NONE;
        changed = true;
    }

    return changed;
}

static void react(pet_t *pet, pet_reaction_t reaction, uint32_t now_ms)
{
    pet->reaction = reaction;
    pet->reaction_until_ms = now_ms + PET_REACTION_MS;
}

bool pet_handle_event(pet_t *pet, pet_event_t event, uint32_t now_ms)
{
    switch (event) {
    case PET_EVENT_POKE:
        pet->pokes++;
        if (pet->mood == PET_MOOD_GRUMPY || pet->mood == PET_MOOD_ME_TIME) {
            react(pet, PET_REACTION_ANNOYED, now_ms);
        } else {
            react(pet, PET_REACTION_SMIRK, now_ms);
        }
        return true;
    case PET_EVENT_SHAKE:
        react(pet, PET_REACTION_DIZZY, now_ms);
        return true;
    }
    return false;
}

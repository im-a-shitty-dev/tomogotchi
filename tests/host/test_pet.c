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

static void test_grumpy_in_the_morning(void)
{
    CHECK(pet_mood_for_hour(5) == PET_MOOD_DEADPAN);
    CHECK(pet_mood_for_hour(6) == PET_MOOD_GRUMPY);
    CHECK(pet_mood_for_hour(9) == PET_MOOD_GRUMPY);
    CHECK(pet_mood_for_hour(10) == PET_MOOD_DEADPAN);
}

static void test_tick_follows_the_clock(void)
{
    pet_t pet;
    pet_init(&pet);
    CHECK(pet_tick(&pet, 7, 0));   // deadpan -> grumpy is a change
    CHECK(pet.mood == PET_MOOD_GRUMPY);
    CHECK(!pet_tick(&pet, 8, 10)); // still grumpy, nothing to redraw
}

static void test_poke_smirks_when_deadpan(void)
{
    pet_t pet;
    pet_init(&pet);
    pet_tick(&pet, 12, 0);
    CHECK(pet_handle_event(&pet, PET_EVENT_POKE, 100));
    CHECK(pet.reaction == PET_REACTION_SMIRK);
    CHECK(pet.pokes == 1);
}

static void test_poke_annoys_when_grumpy(void)
{
    pet_t pet;
    pet_init(&pet);
    pet_tick(&pet, 7, 0);
    pet_handle_event(&pet, PET_EVENT_POKE, 100);
    CHECK(pet.reaction == PET_REACTION_ANNOYED);
}

static void test_reaction_wears_off(void)
{
    pet_t pet;
    pet_init(&pet);
    pet_tick(&pet, 12, 0);
    pet_handle_event(&pet, PET_EVENT_SHAKE, 1000);
    CHECK(pet.reaction == PET_REACTION_DIZZY);
    CHECK(!pet_tick(&pet, 12, 1000 + PET_REACTION_MS - 1));
    CHECK(pet_tick(&pet, 12, 1000 + PET_REACTION_MS));
    CHECK(pet.reaction == PET_REACTION_NONE);
}

static void test_reaction_survives_clock_wrap(void)
{
    pet_t pet;
    pet_init(&pet);
    pet_tick(&pet, 12, 0);
    uint32_t start = UINT32_MAX - 100;
    pet_handle_event(&pet, PET_EVENT_SHAKE, start);
    CHECK(!pet_tick(&pet, 12, start + 50));
    CHECK(pet.reaction == PET_REACTION_DIZZY);
    CHECK(pet_tick(&pet, 12, start + PET_REACTION_MS));
}

static void test_mood_names(void)
{
    CHECK(strcmp(pet_mood_name(PET_MOOD_GRUMPY), "grumpy") == 0);
    CHECK(strcmp(pet_mood_name(PET_MOOD_COUNT), "unknown") == 0);
}

int main(void)
{
    test_grumpy_in_the_morning();
    test_tick_follows_the_clock();
    test_poke_smirks_when_deadpan();
    test_poke_annoys_when_grumpy();
    test_reaction_wears_off();
    test_reaction_survives_clock_wrap();
    test_mood_names();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all pet tests passed\n");
    return EXIT_SUCCESS;
}

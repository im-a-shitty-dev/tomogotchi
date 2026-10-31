// Pet logic: Tomo's state and how he reacts to the world.
//
// Pure C with no ESP-IDF or hardware calls, so it builds and is unit-tested
// on a normal computer (see tests/host). The firmware feeds it input events
// and the local time. The screen calls pet_view() to find out what to draw.
//
// The behaviour follows the design mockup (design/tomo-moods.html).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Time --------------------------------------------------------------------

// Local wall-clock time, as the pet sees it.
typedef struct {
    int month;   // 1-12
    int mday;    // 1-31
    int wday;    // 0 = Sunday ... 6 = Saturday
    int hour;    // 0-23
    int minute;  // 0-59
} pet_clock_t;

// --- What he's doing ---------------------------------------------------------

// The mood, which mostly follows his weekly schedule (see pet_mood_for()).
typedef enum {
    PET_MOOD_ASLEEP = 0,  // tucked in, Zs
    PET_MOOD_GRUMPY,      // just woke up: bed hair, iced coffee
    PET_MOOD_COMMUTE,     // Central line, coat and headphones, scrolling
    PET_MOOD_DEADPAN,     // default: scrolling reels, rare wry smirk
    PET_MOOD_ME_TIME,     // bath, couch + manga, or beach; dislikes pokes
    PET_MOOD_NIGHT,       // on the bed with phone or Switch, screen glow
    PET_MOOD_SPOOKY,      // dressed up in a costume: by swiping, Fri night, 31 Oct
    PET_MOOD_COUNT,
} pet_mood_t;

// Where me-time happens.
typedef enum {
    PET_SCENE_NONE = 0,  // not in me-time
    PET_SCENE_BATH,
    PET_SCENE_COUCH,     // lying down reading manga
    PET_SCENE_BEACH,     // deck chair with a piña colada
    PET_SCENE_COUNT,
} pet_scene_t;

// Where he is, as far as the device can tell (from nearby Wi-Fi). UNKNOWN
// leaves the schedule in charge.
typedef enum {
    PET_PLACE_UNKNOWN = 0,
    PET_PLACE_HOME,        // home network in range: not commuting, whatever the clock says
    PET_PLACE_COMMUTING,   // tube Wi-Fi seen recently: on the train
    PET_PLACE_COUNT,
} pet_place_t;

typedef enum {
    PET_COSTUME_DRACULA = 0,
    PET_COSTUME_CHIIKAWA,
    PET_COSTUME_SADAKO,
    PET_COSTUME_TREX,
    PET_COSTUME_BARREL,
    PET_COSTUME_PIKACHU,
    PET_COSTUME_JASON,
    PET_COSTUME_TRIKE,
    PET_COSTUME_SYLVEON,
    PET_COSTUME_COUNT,
} pet_costume_t;

// What he wears. The renderer picks the body sprite from this.
typedef enum {
    PET_OUTFIT_CASUAL = 0,  // hoodie
    PET_OUTFIT_COAT,        // commute: trench coat + headphones
    PET_OUTFIT_PJS,         // night
    PET_OUTFIT_BATH,        // standing up out of the tub, back to you
    PET_OUTFIT_COSTUME,     // see pet_view_t.costume
    PET_OUTFIT_LYING,       // me-time on the couch or beach (see scene)
    PET_OUTFIT_HIDDEN,      // only his head shows: in the tub, or asleep
    PET_OUTFIT_COUNT,
} pet_outfit_t;

// Face expression. The renderer owns blinking, so there is no blink face.
typedef enum {
    PET_FACE_BLANK = 0,
    PET_FACE_GRUMPY,
    PET_FACE_SMIRK,
    PET_FACE_ANNOYED,
    PET_FACE_DIZZY,
    PET_FACE_HAPPY,
    PET_FACE_FANG,    // smirk with a fang, for horror costumes
    PET_FACE_SLEEP,
    PET_FACE_SCREEN,  // lit by a phone or Switch
    PET_FACE_READ,    // reading manga
    PET_FACE_SHADES,  // sunglasses at the beach
    PET_FACE_COUNT,
} pet_face_t;

// Care items, and what he is holding while eating or drinking one.
typedef enum {
    PET_ITEM_NONE = 0,
    PET_ITEM_GUMMY,
    PET_ITEM_CRISPS,
    PET_ITEM_COFFEE,  // iced
    PET_ITEM_COUNT,
} pet_item_t;

// What he's playing with at night.
typedef enum {
    PET_DEVICE_NONE = 0,
    PET_DEVICE_PHONE,
    PET_DEVICE_SWITCH,
    PET_DEVICE_COUNT,
} pet_device_t;

// --- Input -------------------------------------------------------------------

typedef enum {
    PET_INPUT_TAP = 0,    // single tap on the screen
    PET_INPUT_DOUBLE_TAP, // turn him around
    PET_INPUT_LONG_PRESS, // open the snack picker
    PET_INPUT_SHAKE,      // device shaken (IMU)
    PET_INPUT_GIVE,       // picked an item in the snack picker (see .item)
    PET_INPUT_SWIPE,      // change outfit (see .direction)
} pet_input_t;

typedef struct {
    pet_input_t type;
    pet_item_t item;  // PET_INPUT_GIVE only
    int direction;    // PET_INPUT_SWIPE only: +1 next costume, -1 previous
} pet_event_t;

// --- Tuning ------------------------------------------------------------------

#define PET_HAPPINESS_MAX 5
#define PET_HAPPINESS_START 3
// Happiness drops by one for each stretch this long, awake and ignored.
#define PET_IGNORED_MS (90u * 60u * 1000u)
// Once he's sulking, he needs this much happiness before he'll take a bath...
#define PET_SULK_FED 3
// ...and this long in the bath without being disturbed before he forgives you.
#define PET_SULK_BATH_MS (10u * 60u * 1000u)
// The snack picker closes itself after this long.
#define PET_MENU_MS 6000u

// --- State -------------------------------------------------------------------

// Everything the pet remembers. Treat as private; read it through pet_view().
typedef struct {
    pet_mood_t scheduled;      // mood from the clock alone
    pet_scene_t scene;         // me-time scene from the clock alone
    pet_place_t place;         // where he is; adjusts the scheduled mood
    bool charging;             // on the charger: me-time while awake
    int happiness;             // 0..PET_HAPPINESS_MAX
    pet_costume_t costume;
    pet_device_t device;
    bool dressed_up;           // put in a costume by swiping; off at bedtime
    bool back;                 // turned around by a double-tap
    bool menu_open;
    uint32_t menu_until_ms;
    uint32_t last_care_ms;     // last time anyone interacted, for decay

    // Sulking: he hit zero happiness and turned his back on you.
    bool sulking;
    bool sulk_bathing;         // fed enough; now needs undisturbed bath time
    uint32_t sulk_bath_since_ms;

    // The current short reaction (a face, a line, maybe a snack in hand).
    bool reacting;
    pet_face_t react_face;
    pet_item_t react_item;
    bool react_shake;
    const char *react_line;
    uint32_t react_until_ms;

    uint32_t rng;              // xorshift state for picking lines
    uint32_t pokes;            // lifetime tap count, for fun later
} pet_t;

// --- What to draw ------------------------------------------------------------

// A snapshot of what the screen should show right now. All fields are in the
// pet's own vocabulary; the screen maps them to sprites.
typedef struct {
    pet_mood_t mood;
    pet_scene_t scene;        // set during me-time (and sulk bath)
    pet_outfit_t outfit;
    pet_costume_t costume;    // when outfit is PET_OUTFIT_COSTUME
    pet_face_t face;
    pet_item_t item;          // snack/coffee in hand, or NONE
    pet_device_t device;      // at night: phone or Switch (with screen glow)
    bool phone;               // scrolling reels (deadpan, commute)
    bool cup;                 // morning iced coffee
    bool bedhead;
    bool back;                // facing away (face is not drawn)
    bool shake;               // shaken: jiggle him, splash the bath
    bool menu_open;           // draw the snack picker
    bool sulking;
    int happiness;            // 0..PET_HAPPINESS_MAX
    const char *line;         // UTF-8 speech bubble text, or NULL
} pet_view_t;

// --- API ---------------------------------------------------------------------

// Start a fresh pet. `seed` varies which lines he picks; any value works.
void pet_init(pet_t *pet, uint32_t seed);

// Advance time. `now_ms` is a monotonic clock that may wrap.
// Returns true if anything visible changed and the screen should redraw.
bool pet_tick(pet_t *pet, const pet_clock_t *clock, uint32_t now_ms);

// Tell him where he is. Takes effect on the next pet_tick(). While he's
// awake, COMMUTING puts him on the train (even over a scheduled spooky), and
// HOME during a scheduled commute keeps him at home (deadpan).
void pet_set_place(pet_t *pet, pet_place_t place);

// Tell him whether the device is charging. Takes effect on the next
// pet_tick(). While charging and awake he has me-time (bath on weekdays,
// couch or beach at weekends), over the schedule and the commute. He still
// sleeps at bedtime.
void pet_set_charging(pet_t *pet, bool charging);

// React to an input. Returns true if the screen should redraw.
bool pet_handle_event(pet_t *pet, pet_event_t event, uint32_t now_ms);

// What the screen should draw.
pet_view_t pet_view(const pet_t *pet);

// Every day is Halloween: swiping dresses him up at any time he's awake.
// Swiping steps through the costumes and then back to his normal clothes
// (except when the schedule says spooky, when it just wraps around).
//
// His schedule: the mood and me-time scene for a given time, before any
// interaction. Exposed for tests.
pet_mood_t pet_mood_for(const pet_clock_t *clock);
pet_scene_t pet_scene_for(const pet_clock_t *clock);

bool pet_costume_is_cute(pet_costume_t costume);

const char *pet_mood_name(pet_mood_t mood);
const char *pet_costume_name(pet_costume_t costume);

#ifdef __cplusplus
}
#endif

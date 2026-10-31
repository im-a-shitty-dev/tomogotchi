// What to draw: turns the pet's view and an animation tick into a stack of
// sprites, and renders that stack into an RGB565 buffer.
//
// Plain C with no ESP-IDF or LVGL calls, so it is unit-tested on the host
// (see tests/host). The screen calls scene_build + scene_render once per tick.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pet.h"
#include "sprites_gen.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCENE_W 52
#define SCENE_H 64
#define SCENE_TICK_MS 110  // about 9 fps, like the mockup
#define SCENE_MAX_LAYERS 28  // scene (up to ~13) plus the HUD (up to 12)

typedef struct {
    const sprite_t *sprite;
    uint16_t frame;  // already wrapped to the sprite's frame count
    int8_t x, y;
} scene_layer_t;

typedef struct {
    scene_layer_t layers[SCENE_MAX_LAYERS];  // back to front
    uint8_t count;
    bool dim;  // Tube lights flickering: darken the whole frame
    // Peek strip: before drawing layer `band_from`, darken rows 0..band_rows-1
    // and draw a divider line on the last of them. band_rows 0 means no strip.
    uint8_t band_from;
    uint8_t band_rows;
} scene_t;

// Build the layer stack for `view` at animation tick `tick` (one per
// SCENE_TICK_MS). Blinks, bobbing, the shake jiggle and every fx strip
// follow from the tick.
void scene_build(scene_t *out, const pet_view_t *view, uint32_t tick);

// --- HUD: pop-up hearts and the peek strip -----------------------------------
// These need a little memory between frames (when happiness last changed,
// when the strip was opened), kept here so the timing stays in the scene.

#define SCENE_POP_TICKS 11   // a pop-up heart floats up for this many ticks
#define SCENE_PEEK_TICKS 30  // the peek strip stays about 3.3 s, slide included
#define SCENE_PEEK_ROWS 9    // strip height in art pixels, divider included

typedef struct {
    int last_happiness;  // -1 until the first frame
    bool popping;
    bool pop_up;         // gained a heart (pink) or lost one (broken)
    uint32_t pop_tick;
    bool peeking;
    uint32_t peek_tick;
} scene_hud_t;

// Local time for the clock. hour < 0 means unknown: no digits are drawn.
typedef struct {
    int hour, minute, second;
} scene_time_t;

void scene_hud_init(scene_hud_t *hud);

// Open the peek strip (swipe down). If it is already open it stays open
// for another full stretch, without sliding in again.
void scene_hud_peek(scene_hud_t *hud, uint32_t tick);

// Watch `view` for happiness changes and add the HUD layers on top of a
// built scene: a pop-up heart if happiness just changed, and the peek strip
// with the clock and five hearts if it is open. Call once per frame, after
// scene_build and with the same tick.
void scene_hud(scene_t *s, scene_hud_t *hud, const pet_view_t *view, const scene_time_t *time, uint32_t tick);

// Draw the stack into a SCENE_W x SCENE_H buffer, `stride` pixels per row.
void scene_render(const scene_t *s, uint16_t *buf, int stride);

// The smallest rectangle where frames `a` and `b` differ. Returns false if
// they are identical. Both are SCENE_W x SCENE_H with the given strides.
typedef struct {
    int x, y, w, h;
} scene_rect_t;
bool scene_diff(const uint16_t *a, int a_stride, const uint16_t *b, int b_stride, scene_rect_t *out);

#ifdef __cplusplus
}
#endif

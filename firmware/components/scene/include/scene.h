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
#define SCENE_MAX_LAYERS 12

typedef struct {
    const sprite_t *sprite;
    uint16_t frame;  // already wrapped to the sprite's frame count
    int8_t x, y;
} scene_layer_t;

typedef struct {
    scene_layer_t layers[SCENE_MAX_LAYERS];  // back to front
    uint8_t count;
    bool dim;  // Tube lights flickering: darken the whole frame
} scene_t;

// Build the layer stack for `view` at animation tick `tick` (one per
// SCENE_TICK_MS). Blinks, bobbing, the shake jiggle and every fx strip
// follow from the tick.
void scene_build(scene_t *out, const pet_view_t *view, uint32_t tick);

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

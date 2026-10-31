// Indexed-colour sprites drawn straight into an RGB565 buffer.
//
// The art lives in assets/sprites as PNGs; tools/sprites/build.py turns them
// into sprites_gen.c/.h. Plain C with no ESP-IDF or LVGL calls, so it is
// unit-tested on the host (see tests/host).
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One frame, trimmed to its bounding box. (x, y) is where the box sits on the
// 52x64 canvas when the sprite is drawn at offset (0, 0). px holds w*h
// palette indices, row by row; index 0 is transparent.
typedef struct {
    uint8_t x, y, w, h;
    const uint8_t *px;
} sprite_frame_t;

// A sprite is one or more frames (an animation strip).
typedef struct {
    const sprite_frame_t *frames;
    uint8_t count;
} sprite_t;

typedef struct {
    int8_t x, y;
} sprite_offset_t;

// Draw a frame of `s` into `buf` (RGB565, `stride` pixels per row, `w`x`h`),
// shifted by (dx, dy). The frame index wraps, so a frame counter can be passed
// straight in. Clips to the buffer. NULL sprites draw nothing.
void sprite_draw(uint16_t *buf, int stride, int w, int h,
                 const sprite_t *s, unsigned frame, int dx, int dy);

// One glyph of the speech-bubble font: 11 rows of up to 16 pixels, with
// bit k of a row lighting column k. Draw the next glyph `advance` px on.
#define SPRITE_GLYPH_ROWS 11
typedef struct {
    uint32_t codepoint;
    uint8_t advance;
    uint8_t width;
    uint16_t rows[SPRITE_GLYPH_ROWS];
} sprite_glyph_t;

// The glyph for a Unicode code point, or the font's '?' if it has none.
const sprite_glyph_t *sprite_glyph(uint32_t codepoint);

#ifdef __cplusplus
}
#endif

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

#ifdef __cplusplus
}
#endif

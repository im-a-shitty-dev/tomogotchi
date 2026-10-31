#include "sprite.h"

#include <stddef.h>

#include "sprites_gen.h"

void sprite_draw(uint16_t *buf, int stride, int w, int h,
                 const sprite_t *s, unsigned frame, int dx, int dy)
{
    if (s == NULL || s->count == 0) {
        return;
    }
    const sprite_frame_t *f = &s->frames[frame % s->count];
    int x0 = f->x + dx, y0 = f->y + dy;

    // Clip the frame's box to the buffer once, then copy without per-pixel checks.
    int sx = x0 < 0 ? -x0 : 0;
    int sy = y0 < 0 ? -y0 : 0;
    int ex = x0 + f->w > w ? w - x0 : f->w;
    int ey = y0 + f->h > h ? h - y0 : f->h;

    for (int y = sy; y < ey; y++) {
        const uint8_t *src = f->px + y * f->w;
        uint16_t *dst = buf + (y0 + y) * stride + x0;
        for (int x = sx; x < ex; x++) {
            uint8_t i = src[x];
            if (i) {
                dst[x] = sprite_palette[i];
            }
        }
    }
}

const sprite_glyph_t *sprite_glyph(uint32_t codepoint)
{
    int lo = 0, hi = SPRITE_GLYPH_COUNT - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t c = sprite_glyphs[mid].codepoint;
        if (c == codepoint) {
            return &sprite_glyphs[mid];
        }
        if (c < codepoint) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return codepoint == '?' ? NULL : sprite_glyph('?');
}

#include <stdio.h>
#include <string.h>

#include "sprites_gen.h"

static int failures = 0;

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,    \
                    __LINE__, #cond);                                 \
            failures++;                                               \
        }                                                             \
    } while (0)

#define CW 52
#define CH 64

static void check_sprite(const sprite_t *s)
{
    CHECK(s->count > 0);
    for (int n = 0; n < s->count; n++) {
        const sprite_frame_t *f = &s->frames[n];
        CHECK(f->x + f->w <= CW);
        CHECK(f->y + f->h <= CH);
        for (int i = 0; i < f->w * f->h; i++) {
            CHECK(f->px[i] < SPRITE_PALETTE_SIZE);
        }
    }
}

static void test_generated_data_fits_the_canvas(void)
{
    CHECK(sprite_palette[0] == 0);
    for (int i = 0; i < SPRITE_BG_COUNT; i++) check_sprite(&sprite_bg[i]);
    for (int i = 0; i < SPRITE_BODY_COUNT; i++) {
        check_sprite(&sprite_body[i]);
        for (int f = 0; f < SPRITE_FACE_COUNT; f++) {
            const sprite_t *s = sprite_face(i, f);
            if (s) check_sprite(s);
        }
        for (int p = 0; p < SPRITE_PROP_COUNT; p++) {
            const sprite_t *s = sprite_prop(i, p);
            if (s) check_sprite(s);
        }
    }
    for (int i = 0; i < SPRITE_FRONT_COUNT; i++) check_sprite(&sprite_front[i]);
}

static void test_backgrounds_cover_the_canvas(void)
{
    for (int i = 0; i < SPRITE_BG_COUNT; i++) {
        const sprite_frame_t *f = &sprite_bg[i].frames[0];
        CHECK(f->x == 0 && f->y == 0 && f->w == CW && f->h == CH);
    }
}

static void test_face_overrides(void)
{
    // Most bodies share the plain faces.
    CHECK(sprite_face(SPRITE_BODY_CASUAL, SPRITE_FACE_BLANK) != NULL);
    CHECK(sprite_face(SPRITE_BODY_DRACULA, SPRITE_FACE_SMIRK) ==
          sprite_face(SPRITE_BODY_CASUAL, SPRITE_FACE_SMIRK));
    // Sadako's hair hides her face, apart from a blink.
    CHECK(sprite_face(SPRITE_BODY_SADAKO, SPRITE_FACE_BLANK) == NULL);
    CHECK(sprite_face(SPRITE_BODY_SADAKO, SPRITE_FACE_BLINK) != NULL);
    // The Chiikawa mask has its own art.
    CHECK(sprite_face(SPRITE_BODY_CHIIKAWA, SPRITE_FACE_SMIRK) !=
          sprite_face(SPRITE_BODY_CASUAL, SPRITE_FACE_SMIRK));
    // Out of range is NULL, not a crash.
    CHECK(sprite_face(SPRITE_BODY_COUNT, SPRITE_FACE_BLANK) == NULL);
    CHECK(sprite_prop(SPRITE_BODY_CASUAL, SPRITE_PROP_COUNT) == NULL);
}

// A 3x2 test sprite: index 0 is a hole that must not be painted.
static const uint8_t tiny_px[] = {1, 0, 2, 2, 2, 1};
static const sprite_frame_t tiny_frames[] = {{10, 20, 3, 2, tiny_px}, {0, 0, 0, 0, NULL}};
static const sprite_t tiny = {tiny_frames, 2};

// Canvas with a guard band so out-of-bounds writes are caught.
#define GUARD 8
#define STRIDE (CW + 2 * GUARD)
static uint16_t mem[(CH + 2 * GUARD) * STRIDE];
static uint16_t *const canvas = mem + GUARD * STRIDE + GUARD;
#define AT(x, y) canvas[(y) * STRIDE + (x)]
#define BG 0xBEEF

static void clear(void)
{
    for (size_t i = 0; i < sizeof mem / sizeof mem[0]; i++) mem[i] = BG;
}

static int painted_outside(void)
{
    int n = 0;
    for (int y = -GUARD; y < CH + GUARD; y++)
        for (int x = -GUARD; x < CW + GUARD; x++)
            if ((x < 0 || y < 0 || x >= CW || y >= CH) && AT(x, y) != BG) n++;
    return n;
}

static void test_draw_skips_transparent_pixels(void)
{
    clear();
    sprite_draw(canvas, STRIDE, CW, CH, &tiny, 0, 1, 1);
    CHECK(AT(11, 21) == sprite_palette[1]);
    CHECK(AT(12, 21) == BG);
    CHECK(AT(13, 21) == sprite_palette[2]);
    CHECK(AT(13, 22) == sprite_palette[1]);
    CHECK(AT(10, 20) == BG);
}

static void test_draw_clips_at_every_edge(void)
{
    clear();
    sprite_draw(canvas, STRIDE, CW, CH, &tiny, 0, -11, -20);  // top-left, 2 px left
    CHECK(AT(0, 0) == BG);                 // was the hole
    CHECK(AT(1, 0) == sprite_palette[2]);
    CHECK(AT(0, 1) == sprite_palette[2]);
    sprite_draw(canvas, STRIDE, CW, CH, &tiny, 0, CW - 11, CH - 21);  // bottom-right
    CHECK(AT(CW - 1, CH - 1) == sprite_palette[1]);  // only the top-left pixel fits
    sprite_draw(canvas, STRIDE, CW, CH, &tiny, 0, -100, 0);  // fully off
    sprite_draw(canvas, STRIDE, CW, CH, &tiny, 0, 0, 100);
    for (int i = 0; i < SPRITE_BODY_COUNT; i++) {
        sprite_draw(canvas, STRIDE, CW, CH, &sprite_body[i], 0, -30, 40);
        sprite_draw(canvas, STRIDE, CW, CH, &sprite_body[i], 0, 30, -40);
    }
    CHECK(painted_outside() == 0);
}

static void test_draw_wraps_frames_and_ignores_null(void)
{
    clear();
    sprite_draw(canvas, STRIDE, CW, CH, &tiny, 1, 0, 0);  // empty frame
    sprite_draw(canvas, STRIDE, CW, CH, NULL, 0, 0, 0);
    CHECK(AT(10, 20) == BG);
    sprite_draw(canvas, STRIDE, CW, CH, &tiny, 2, 0, 0);  // wraps to frame 0
    CHECK(AT(10, 20) == sprite_palette[1]);
}

int main(void)
{
    test_generated_data_fits_the_canvas();
    test_backgrounds_cover_the_canvas();
    test_face_overrides();
    test_draw_skips_transparent_pixels();
    test_draw_clips_at_every_edge();
    test_draw_wraps_frames_and_ignores_null();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all sprite tests passed\n");
    return 0;
}

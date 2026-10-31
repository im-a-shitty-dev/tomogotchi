#include "scene.h"

#include <stddef.h>

// --- Timing, in ticks of SCENE_TICK_MS (from the mockup) ---------------------

#define BLINK_EVERY 32  // blink for 2 ticks out of every 32 (~3.5 s)
#define BLINK_LEN 2
#define BOB_EVERY 5      // idle bob: up/down 1px every 5 ticks
#define SLEEP_BOB_EVERY 12
#define WAVES_EVERY 3    // the waves strip was sampled every 3 ticks
#define SWAY_EVERY 40    // the train sways for 3 ticks every 40
#define SWAY_LEN 3
#define DIM_EVERY 150    // Tube lights dip on ticks 0, 1 and 4 of every 150

// --- Mapping the pet's view to sprites ---------------------------------------

static sprite_body_t costume_body(pet_costume_t c, bool back)
{
    static const sprite_body_t front[PET_COSTUME_COUNT] = {
        [PET_COSTUME_DRACULA] = SPRITE_BODY_DRACULA, [PET_COSTUME_CHIIKAWA] = SPRITE_BODY_CHIIKAWA,
        [PET_COSTUME_SADAKO] = SPRITE_BODY_SADAKO,   [PET_COSTUME_TREX] = SPRITE_BODY_TREX,
        [PET_COSTUME_BARREL] = SPRITE_BODY_BARREL,   [PET_COSTUME_PIKACHU] = SPRITE_BODY_PIKACHU,
        [PET_COSTUME_JASON] = SPRITE_BODY_JASON,     [PET_COSTUME_TRIKE] = SPRITE_BODY_TRIKE,
        [PET_COSTUME_SYLVEON] = SPRITE_BODY_SYLVEON,
    };
    static const sprite_body_t rear[PET_COSTUME_COUNT] = {
        [PET_COSTUME_DRACULA] = SPRITE_BODY_DRACULA_BACK, [PET_COSTUME_CHIIKAWA] = SPRITE_BODY_CHIIKAWA_BACK,
        [PET_COSTUME_SADAKO] = SPRITE_BODY_SADAKO_BACK,   [PET_COSTUME_TREX] = SPRITE_BODY_TREX_BACK,
        [PET_COSTUME_BARREL] = SPRITE_BODY_BARREL_BACK,   [PET_COSTUME_PIKACHU] = SPRITE_BODY_PIKACHU_BACK,
        [PET_COSTUME_JASON] = SPRITE_BODY_JASON_BACK,     [PET_COSTUME_TRIKE] = SPRITE_BODY_TRIKE_BACK,
        [PET_COSTUME_SYLVEON] = SPRITE_BODY_SYLVEON_BACK,
    };
    return back ? rear[c] : front[c];
}

static sprite_body_t body_for(const pet_view_t *v)
{
    switch (v->outfit) {
    case PET_OUTFIT_COAT: return v->back ? SPRITE_BODY_CASUAL_BACK : SPRITE_BODY_COAT;  // no coat back yet
    case PET_OUTFIT_PJS:
        return v->back                           ? SPRITE_BODY_PJS_BACK
               : v->device == PET_DEVICE_PHONE  ? SPRITE_BODY_PJS_PHONE
               : v->device == PET_DEVICE_SWITCH ? SPRITE_BODY_PJS_SWITCH
                                                : SPRITE_BODY_PJS;
    case PET_OUTFIT_BATH: return SPRITE_BODY_BATH_BACK;
    case PET_OUTFIT_COSTUME: return costume_body(v->costume, v->back);
    case PET_OUTFIT_LYING: return v->scene == PET_SCENE_BEACH ? SPRITE_BODY_BEACH : SPRITE_BODY_COUCH;
    case PET_OUTFIT_HIDDEN: return SPRITE_BODY_HEAD;
    default: return v->back ? SPRITE_BODY_CASUAL_BACK : SPRITE_BODY_CASUAL;
    }
}

// The face to show at this tick: resting faces blink now and then.
static sprite_face_t face_for(pet_face_t f, uint32_t tick)
{
    static const sprite_face_t faces[PET_FACE_COUNT] = {
        [PET_FACE_BLANK] = SPRITE_FACE_BLANK,   [PET_FACE_GRUMPY] = SPRITE_FACE_GRUMPY,
        [PET_FACE_SMIRK] = SPRITE_FACE_SMIRK,   [PET_FACE_ANNOYED] = SPRITE_FACE_ANNOYED,
        [PET_FACE_DIZZY] = SPRITE_FACE_DIZZY,   [PET_FACE_HAPPY] = SPRITE_FACE_HAPPY,
        [PET_FACE_FANG] = SPRITE_FACE_FANG,     [PET_FACE_SLEEP] = SPRITE_FACE_SLEEP,
        [PET_FACE_SCREEN] = SPRITE_FACE_SCREEN, [PET_FACE_READ] = SPRITE_FACE_READ,
        [PET_FACE_SHADES] = SPRITE_FACE_SHADES,
    };
    if ((unsigned)f >= PET_FACE_COUNT) {
        return SPRITE_FACE_BLANK;
    }
    if (tick % BLINK_EVERY < BLINK_LEN) {
        switch (f) {
        case PET_FACE_GRUMPY: return SPRITE_FACE_GRUMPYBLINK;
        case PET_FACE_BLANK:
        case PET_FACE_SCREEN:
        case PET_FACE_READ: return SPRITE_FACE_BLINK;
        default: break;
        }
    }
    return faces[f];
}

static void push(scene_t *s, const sprite_t *sprite, uint32_t frame, int x, int y)
{
    if (sprite == NULL || s->count >= SCENE_MAX_LAYERS) {
        return;
    }
    s->layers[s->count++] = (scene_layer_t){sprite, (uint16_t)(frame % sprite->count), (int8_t)x, (int8_t)y};
}

void scene_build(scene_t *s, const pet_view_t *v, uint32_t t)
{
    s->count = 0;
    s->dim = false;

    sprite_body_t body = body_for(v);
    sprite_bg_t bg = SPRITE_BG_DEADPAN;
    int fx = -1, front = -1, behind = -1;
    int dx = 0, dy = (t / BOB_EVERY) % 2;  // idle bob
    bool bath = false, zzz = false, bats = false;

    switch (v->mood) {
    case PET_MOOD_ASLEEP:
        bg = SPRITE_BG_ASLEEP;
        front = SPRITE_FRONT_BLANKET;
        dy = 6 + (t / SLEEP_BOB_EVERY) % 2;  // slow breathing
        zzz = !v->shake && v->line == NULL;  // Zs stop while he's reacting
        break;
    case PET_MOOD_GRUMPY: bg = SPRITE_BG_MORNING; break;
    case PET_MOOD_COMMUTE:
        bg = SPRITE_BG_COMMUTE;
        fx = SPRITE_FX_TUNNEL;
        dy = 1;
        if (t % SWAY_EVERY < SWAY_LEN) dx = t % 2;  // the carriage lurches
        s->dim = t % DIM_EVERY == 0 || t % DIM_EVERY == 1 || t % DIM_EVERY == 4;
        break;
    case PET_MOOD_NIGHT: bg = SPRITE_BG_NIGHT; dy = 0; break;
    case PET_MOOD_SPOOKY:
        bg = SPRITE_BG_SPOOKY;
        fx = SPRITE_FX_STARS;
        bats = true;
        break;
    case PET_MOOD_ME_TIME:
        if (v->scene == PET_SCENE_COUCH) {
            bg = SPRITE_BG_COUCH;
            dy = 0;
        } else if (v->scene == PET_SCENE_BEACH) {
            bg = SPRITE_BG_BEACH;
            fx = SPRITE_FX_WAVES;
            dy = 0;
        } else {
            bg = SPRITE_BG_BATH;
            bath = true;
            if (v->outfit == PET_OUTFIT_BATH) {
                behind = SPRITE_FRONT_TUB;  // stood up, back to you: tub behind him
            } else {
                front = SPRITE_FRONT_TUB;
                dy += 7;  // sitting in it
            }
        }
        break;
    default: break;
    }
    if (v->shake) dx = t % 2 ? 1 : -1;  // jiggle

    // Background, then anything moving in it.
    push(s, &sprite_bg[bg], 0, 0, 0);
    if (fx >= 0) push(s, &sprite_fx[fx], fx == SPRITE_FX_WAVES ? t / WAVES_EVERY : t, 0, 0);
    if (bats) {
        // Two bats on one strip: the second slower, lower and further along.
        push(s, &sprite_fx[SPRITE_FX_BAT], t, 0, 0);
        push(s, &sprite_fx[SPRITE_FX_BAT], t * 7 / 10 + 28, 0, 7);
    }
    if (behind >= 0) push(s, &sprite_front[behind], 0, 0, 0);

    // Tomo.
    push(s, &sprite_body[body], 0, dx, dy);
    int hx = dx + sprite_head_offset[body].x, hy = dy + sprite_head_offset[body].y;
    if (!v->back) {
        push(s, sprite_face(body, face_for(v->face, t)), 0, hx, hy);
        if (v->bedhead) push(s, sprite_prop(body, SPRITE_PROP_BEDHEAD), 0, hx, hy);
        static const int items[PET_ITEM_COUNT] = {
            [PET_ITEM_NONE] = -1, [PET_ITEM_GUMMY] = SPRITE_PROP_GUMMY,
            [PET_ITEM_CRISPS] = SPRITE_PROP_CRISPS, [PET_ITEM_COFFEE] = SPRITE_PROP_COFFEE,
        };
        int held = (unsigned)v->item < PET_ITEM_COUNT ? items[v->item] : -1;
        if (held < 0 && v->cup) held = SPRITE_PROP_CUP;
        if (held < 0 && v->phone) held = SPRITE_PROP_PHONE;
        if (held >= 0) push(s, sprite_prop(body, held), 0, hx, hy);
    }

    // In front of him.
    if (front >= 0) push(s, &sprite_front[front], 0, 0, 0);
    if (bath && front >= 0) {
        if (v->shake) push(s, &sprite_fx[SPRITE_FX_SPLASH], t, 0, 0);
        else push(s, &sprite_fx[SPRITE_FX_STEAM], t, 0, 0);
    }
    if (zzz) push(s, &sprite_fx[SPRITE_FX_ZZZ], t, 0, 0);
}

// Darken an RGB565 pixel to about 45%, like the mockup's flicker.
static uint16_t dim565(uint16_t c)
{
    unsigned r = (c >> 11) & 0x1f, g = (c >> 5) & 0x3f, b = c & 0x1f;
    r = r * 115 / 256;
    g = g * 115 / 256;
    b = b * 115 / 256;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void scene_render(const scene_t *s, uint16_t *buf, int stride)
{
    for (int i = 0; i < s->count; i++) {
        const scene_layer_t *l = &s->layers[i];
        sprite_draw(buf, stride, SCENE_W, SCENE_H, l->sprite, l->frame, l->x, l->y);
    }
    if (s->dim) {
        for (int y = 0; y < SCENE_H; y++) {
            for (int x = 0; x < SCENE_W; x++) {
                buf[y * stride + x] = dim565(buf[y * stride + x]);
            }
        }
    }
}

bool scene_diff(const uint16_t *a, int a_stride, const uint16_t *b, int b_stride, scene_rect_t *out)
{
    int x0 = SCENE_W, y0 = SCENE_H, x1 = -1, y1 = -1;
    for (int y = 0; y < SCENE_H; y++) {
        const uint16_t *ra = a + y * a_stride, *rb = b + y * b_stride;
        for (int x = 0; x < SCENE_W; x++) {
            if (ra[x] != rb[x]) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                y1 = y;
            }
        }
    }
    if (x1 < 0) {
        return false;
    }
    *out = (scene_rect_t){x0, y0, x1 - x0 + 1, y1 - y0 + 1};
    return true;
}

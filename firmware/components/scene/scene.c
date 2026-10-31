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
#define PEEK_SLIDE 3     // the peek strip slides 3 px per tick, in and out
#define PEEK_DIVIDER 0x5a4d  // #5a4a6a, the line under the peek strip
#define ANGER_EVERY 10   // the sulk anger mark shows on 7 ticks of every 10
#define ANGER_ON 7

#define RGB565(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define BUBBLE_FILL RGB565(0xfb, 0xfa, 0xf6)
#define BUBBLE_EDGE RGB565(0x07, 0x05, 0x0b)  // the art's outline colour
#define BUBBLE_INK RGB565(0x12, 0x0e, 0x18)

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
    s->band_from = 0;
    s->band_rows = 0;
    s->bubble_text = NULL;
    s->bubble_at = 0;

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
    s->head_dy = (int8_t)dy;
    s->lying = v->outfit == PET_OUTFIT_LYING;
    if (v->shake) dx = t % 2 ? 1 : -1;  // jiggle

    // Background, then anything moving in it.
    push(s, &sprite_bg[bg], 0, 0, 0);
    if (fx >= 0) push(s, &sprite_fx[fx], fx == SPRITE_FX_WAVES ? t / WAVES_EVERY : t, 0, 0);
    if (bats) {
        // Two bats on one strip: the second slower, lower and further along.
        push(s, &sprite_fx[SPRITE_FX_BAT], t, 0, 0);
        push(s, &sprite_fx[SPRITE_FX_BAT], t * 7 / 10 + 28, 0, 7);
    }
    if (behind >= 0) {
        push(s, &sprite_front[behind], 0, 0, 0);
        push(s, &sprite_fx[SPRITE_FX_FOAM], t, 0, 0);
    }

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
        push(s, &sprite_fx[SPRITE_FX_FOAM], t, 0, 0);  // bubbles overflowing
        if (v->shake) push(s, &sprite_fx[SPRITE_FX_SPLASH], t, 0, 0);
        else push(s, &sprite_fx[SPRITE_FX_STEAM], t, 0, 0);
    }
    if (zzz) push(s, &sprite_fx[SPRITE_FX_ZZZ], t, 0, 0);

    // His mood in the scene: a rain cloud at no hearts, sparkles at full.
    if (v->happiness <= 0 && v->mood != PET_MOOD_ASLEEP) push(s, &sprite_fx[SPRITE_FX_RAIN], t, 0, 0);
    if (v->happiness >= PET_HAPPINESS_MAX) push(s, &sprite_fx[SPRITE_FX_SPARKLE], t, 0, 0);
}

// --- HUD ---------------------------------------------------------------------

// The emote that goes with a face (from the mockup's emoteFor). Faces with
// nothing to add get the dots, which the bubble makes redundant.
static sprite_hud_t emote_for(const pet_view_t *v)
{
    if (v->item != PET_ITEM_NONE) return SPRITE_HUD_EMOTE_HEART;
    switch (v->face) {
    case PET_FACE_ANNOYED:
    case PET_FACE_GRUMPY: return SPRITE_HUD_EMOTE_VEIN;
    case PET_FACE_SLEEP: return SPRITE_HUD_EMOTE_ZZZ;
    case PET_FACE_DIZZY: return SPRITE_HUD_EMOTE_SWIRL;
    case PET_FACE_FANG: return SPRITE_HUD_EMOTE_BANG;
    case PET_FACE_SMIRK:
    case PET_FACE_SHADES: return SPRITE_HUD_EMOTE_NOTE;
    case PET_FACE_HAPPY: return SPRITE_HUD_EMOTE_HEART;
    default: return SPRITE_HUD_EMOTE_DOTS;
    }
}

// Emote art sits at (1, 1) inside its outline, so place it one up and left.
static void push_emote(scene_t *s, sprite_hud_t e, int x, int y)
{
    push(s, &sprite_hud[e], 0, x - 1, y - 1);
}

// His line in a bubble, with an emote by his head. While he says nothing,
// a blinking anger mark shows he's sulking or has no hearts left.
static void speech(scene_t *s, scene_hud_t *hud, const pet_view_t *v, uint32_t t)
{
    bool asleep = v->mood == PET_MOOD_ASLEEP;
    // His face is low in the tub (unless he's stood up), lying down, or in
    // bed, so the bubble goes at the top instead of over his body.
    bool low = (v->mood == PET_MOOD_ME_TIME && v->outfit != PET_OUTFIT_BATH) || asleep;
    bool upset = (v->sulking || v->happiness <= 0) && !asleep;
    int head_x = s->lying ? 15 : 26;

    if (v->line == NULL) {
        hud->line = NULL;
        if (upset && t % ANGER_EVERY < ANGER_ON) {
            push_emote(s, SPRITE_HUD_EMOTE_VEIN, head_x + 10, s->lying ? 14 : 8 + s->head_dy);
        }
        return;
    }
    if (v->line != hud->line) {
        hud->line = v->line;
        hud->line_tick = t;
    }
    uint32_t k = t - hud->line_tick;
    s->bubble_text = v->line;
    s->bubble_at = s->count;
    s->bubble_top = low;
    s->bubble_tail_x = (int8_t)(head_x + 2);
    s->bubble_ticks = (uint16_t)(k > 0xffff ? 0xffff : k);

    sprite_hud_t e = emote_for(v);
    if (e == SPRITE_HUD_EMOTE_DOTS) {
        if (!upset) return;
        e = SPRITE_HUD_EMOTE_VEIN;
    }
    // Beside his head, or below a top bubble.
    int x = s->lying && low ? 2 : head_x + (low ? 13 : 10);
    int y = s->lying ? (low ? 26 : 14) : (low ? 30 : 6) + s->head_dy;
    push_emote(s, e, x, y);
}

void scene_hud_init(scene_hud_t *hud)
{
    *hud = (scene_hud_t){.last_happiness = -1};
}

void scene_hud_peek(scene_hud_t *hud, uint32_t t)
{
    if (hud->peeking && t - hud->peek_tick < SCENE_PEEK_TICKS) {
        // Already open: keep it open for another stretch, without sliding
        // in again (unless it is still sliding in).
        if (t - hud->peek_tick > PEEK_SLIDE) hud->peek_tick = t - PEEK_SLIDE;
        return;
    }
    hud->peeking = true;
    hud->peek_tick = t;
}

// How far the strip is pushed up off the top: -SCENE_PEEK_ROWS (hidden) to 0
// (fully down). It slides in over the first ticks and out over the last.
static int peek_offset(uint32_t k)
{
    int in = (int)k * PEEK_SLIDE - SCENE_PEEK_ROWS;
    int out = (SCENE_PEEK_TICKS - (int)k) * PEEK_SLIDE - SCENE_PEEK_ROWS;
    int off = in < out ? in : out;
    return off > 0 ? 0 : off < -SCENE_PEEK_ROWS ? -SCENE_PEEK_ROWS : off;
}

void scene_hud(scene_t *s, scene_hud_t *hud, const pet_view_t *v, const scene_time_t *time, uint32_t t)
{
    // Pop-up heart when happiness changes. A new change restarts it.
    if (hud->last_happiness >= 0 && v->happiness != hud->last_happiness) {
        hud->popping = true;
        hud->pop_up = v->happiness > hud->last_happiness;
        hud->pop_tick = t;
    }
    hud->last_happiness = v->happiness;
    if (hud->popping && t - hud->pop_tick >= SCENE_POP_TICKS) hud->popping = false;
    if (hud->popping) {
        uint32_t k = t - hud->pop_tick;
        // Broken hearts are blue, but violet against the blue beach sky.
        bool beach = v->mood == PET_MOOD_ME_TIME && v->scene == PET_SCENE_BEACH;
        sprite_hud_t pop = hud->pop_up ? SPRITE_HUD_POP_UP : beach ? SPRITE_HUD_POP_DOWN_BEACH : SPRITE_HUD_POP_DOWN;
        push(s, &sprite_hud[pop], 0, k % 4 >= 2 ? 1 : 0, -(int)k);  // float up, wobbling
    }

    speech(s, hud, v, t);

    // Peek strip: clock on the left, hearts on the right, over a dark band.
    if (hud->peeking && t - hud->peek_tick >= SCENE_PEEK_TICKS) hud->peeking = false;
    if (!hud->peeking) {
        return;
    }
    int off = peek_offset(t - hud->peek_tick);
    if (off <= -SCENE_PEEK_ROWS) {
        return;
    }
    s->band_from = s->count;
    s->band_rows = (uint8_t)(SCENE_PEEK_ROWS + off);
    if (time != NULL && time->hour >= 0) {
        static const int8_t slot[4] = {0, 4, 10, 14};  // HH:MM
        int digits[4] = {time->hour / 10 % 10, time->hour % 10, time->minute / 10 % 10, time->minute % 10};
        for (int i = 0; i < 4; i++) push(s, &sprite_hud[SPRITE_HUD_DIGITS], digits[i], slot[i], off);
        if (time->second % 2 == 0) push(s, &sprite_hud[SPRITE_HUD_COLON], 0, 0, off);
    }
    for (int i = 0; i < PET_HAPPINESS_MAX; i++) {
        push(s, &sprite_hud[i < v->happiness ? SPRITE_HUD_HEART : SPRITE_HUD_HEART_EMPTY], 0, i * 4, off);
    }
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

// Darken the top rows behind the peek strip, with a divider line under it.
static void draw_band(uint16_t *buf, int stride, int rows)
{
    for (int y = 0; y < rows && y < SCENE_H; y++) {
        for (int x = 0; x < SCENE_W; x++) {
            uint16_t *p = &buf[y * stride + x];
            *p = y == rows - 1 ? PEEK_DIVIDER : dim565(*p);
        }
    }
}

// --- Speech bubble -----------------------------------------------------------

// Decode one UTF-8 character and step past it. Bad bytes come out as '?'.
static uint32_t utf8_next(const char **p)
{
    const unsigned char *u = (const unsigned char *)*p;
    uint32_t c = u[0];
    int extra = c < 0x80 ? 0 : (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : (c & 0xf8) == 0xf0 ? 3 : -1;
    if (extra < 0) {
        *p += 1;
        return '?';
    }
    if (extra > 0) c &= 0x3f >> extra;
    for (int i = 1; i <= extra; i++) {
        if ((u[i] & 0xc0) != 0x80) {
            *p += i;
            return '?';
        }
        c = (c << 6) | (u[i] & 0x3f);
    }
    *p += 1 + extra;
    return c;
}

static int text_width(const uint32_t *cps, int from, int to)
{
    int w = 0;
    for (int i = from; i < to; i++) w += sprite_glyph(cps[i])->advance;
    return w;
}

static int add_line(const uint32_t *cps, int from, int to, scene_text_line_t *lines, int n, int max_lines)
{
    while (from < to && cps[from] == ' ') from++;
    while (to > from && cps[to - 1] == ' ') to--;
    if (from == to || n >= max_lines) return n;
    lines[n] = (scene_text_line_t){(uint8_t)from, (uint8_t)(to - from), (uint8_t)text_width(cps, from, to)};
    return n + 1;
}

// The mockup's wrapText: English breaks at the last space, Japanese (and
// words too long for a line) break anywhere.
int scene_wrap(const char *text, int max_w, uint32_t *cps, int max_cps, scene_text_line_t *lines, int max_lines)
{
    int n = 0;
    if (max_cps > 255) max_cps = 255;  // ranges are 8-bit
    while (text != NULL && *text && n < max_cps) cps[n++] = utf8_next(&text);

    int count = 0, from = 0;  // the line so far is cps[from..i)
    for (int i = 0; i < n; i++) {
        if (from == i || text_width(cps, from, i + 1) <= max_w) continue;
        int sp = -1;
        for (int j = i - 1; j > from; j--) {
            if (cps[j] == ' ') {
                sp = j;
                break;
            }
        }
        bool ascii = cps[i] >= 0x20 && cps[i] <= 0x7e;
        if (ascii && sp > from) {
            count = add_line(cps, from, sp, lines, count, max_lines);
            from = sp + 1;
        } else {
            count = add_line(cps, from, i, lines, count, max_lines);
            from = cps[i] == ' ' ? i + 1 : i;
        }
    }
    return add_line(cps, from, n, lines, count, max_lines);
}

#define BUBBLE_MAX_CPS 96
#define BUBBLE_MAX_LINES 8

// A white box with a 1px outline and a tail pointing at his head, showing two
// rows of the line at a time. Long lines page through every SCENE_PAGE_TICKS.
static void draw_bubble(const scene_t *s, uint16_t *buf, int stride)
{
    // Static rather than ~1 KB on the caller's stack; rendering is one task.
    static uint32_t cps[BUBBLE_MAX_CPS];
    static uint64_t mask[SCENE_H];
    scene_text_line_t lines[BUBBLE_MAX_LINES];
    int n = scene_wrap(s->bubble_text, SCENE_BUBBLE_WRAP, cps, BUBBLE_MAX_CPS, lines, BUBBLE_MAX_LINES);
    if (n == 0) return;
    int pages = (n + 1) / 2;
    int page = s->bubble_ticks / SCENE_PAGE_TICKS;
    if (page > pages - 1) page = pages - 1;
    int first = page * 2, rows = n - first < 2 ? n - first : 2;

    int bw = 0;
    for (int i = first; i < first + rows; i++) {
        if (lines[i].width > bw) bw = lines[i].width;
    }
    bw += 4;
    int bh = rows * SPRITE_GLYPH_ROWS + 2;
    int x0 = (SCENE_W - bw + 1) / 2;
    if (x0 > SCENE_W - 1 - bw) x0 = SCENE_W - 1 - bw;
    if (x0 < 1) x0 = 1;
    int y0 = s->bubble_top ? 1 : SCENE_H - 2 - bh;
    int tx = s->bubble_tail_x;
    if (tx > x0 + bw - 4) tx = x0 + bw - 4;
    if (tx < x0 + 2) tx = x0 + 2;

    // The bubble's shape as a bit mask per row, then outline it like the art.
    for (int y = 0; y < SCENE_H; y++) mask[y] = 0;
    for (int y = y0; y < y0 + bh; y++) mask[y] = ((1ull << bw) - 1) << x0;
    int t1 = s->bubble_top ? y0 + bh : y0 - 1, t2 = s->bubble_top ? y0 + bh + 1 : y0 - 2;
    if (t1 >= 0 && t1 < SCENE_H) mask[t1] |= 7ull << tx;
    if (t2 >= 0 && t2 < SCENE_H) mask[t2] |= 1ull << (tx + 1);
    const uint64_t row_bits = (1ull << SCENE_W) - 1;
    for (int y = 0; y < SCENE_H; y++) {
        uint64_t near = (mask[y] << 1) | (mask[y] >> 1);
        if (y > 0) near |= mask[y - 1];
        if (y < SCENE_H - 1) near |= mask[y + 1];
        uint64_t edge = near & ~mask[y] & row_bits;
        for (int x = 0; x < SCENE_W; x++) {
            if (mask[y] >> x & 1) buf[y * stride + x] = BUBBLE_FILL;
            else if (edge >> x & 1) buf[y * stride + x] = BUBBLE_EDGE;
        }
    }

    for (int i = 0; i < rows; i++) {
        const scene_text_line_t *ln = &lines[first + i];
        int x = x0 + 2 + (bw - 4 - ln->width) / 2;
        int y = y0 + 1 + i * SPRITE_GLYPH_ROWS;
        for (int c = ln->start; c < ln->start + ln->len; c++) {
            const sprite_glyph_t *g = sprite_glyph(cps[c]);
            for (int r = 0; r < SPRITE_GLYPH_ROWS; r++) {
                for (int k = 0; k < g->width; k++) {
                    int px = x + k, py = y + r;
                    if ((g->rows[r] >> k & 1) && px >= 0 && px < SCENE_W && py >= 0 && py < SCENE_H) {
                        buf[py * stride + px] = BUBBLE_INK;
                    }
                }
            }
            x += g->advance;
        }
    }
}

void scene_render(const scene_t *s, uint16_t *buf, int stride)
{
    for (int i = 0; i < s->count; i++) {
        if (s->bubble_text && i == s->bubble_at) draw_bubble(s, buf, stride);
        if (s->band_rows && i == s->band_from) draw_band(buf, stride, s->band_rows);
        const scene_layer_t *l = &s->layers[i];
        sprite_draw(buf, stride, SCENE_W, SCENE_H, l->sprite, l->frame, l->x, l->y);
    }
    if (s->bubble_text && s->bubble_at >= s->count) draw_bubble(s, buf, stride);
    if (s->band_rows && s->band_from >= s->count) draw_band(buf, stride, s->band_rows);
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

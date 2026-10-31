#include "screen.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"
#include "sprites_gen.h"

static const char *TAG = "screen";

static lv_obj_t *s_canvas;
static QueueHandle_t s_events;

// RGB565 canvas buffer: 52 * 64 * 2 bytes, about 6.6 KB.
LV_DRAW_BUF_DEFINE_STATIC(s_canvas_buf, SCREEN_ART_W, SCREEN_ART_H, LV_COLOR_FORMAT_RGB565);

// --- Input ------------------------------------------------------------------
// A tap waits briefly in case a second one makes it a double-tap (turn him
// around). A long press opens the snack picker; while it is open, a tap picks
// the item under your finger (left, middle or right third). Swipes change
// his outfit.

#define DOUBLE_TAP_MS 280

static lv_timer_t *s_tap_timer;
static bool s_tap_pending;  // a tap is waiting to see if a second one follows
static volatile bool s_menu_open;

static void cancel_tap(void)
{
    lv_timer_pause(s_tap_timer);
    s_tap_pending = false;
}

static void post(pet_event_t ev)
{
    xQueueSend(s_events, &ev, 0);
}

static void on_single_tap(lv_timer_t *t)
{
    (void)t;
    cancel_tap();
    post((pet_event_t){.type = PET_INPUT_TAP});
}

static void on_tap(lv_event_t *e)
{
    (void)e;
    if (s_menu_open) {
        lv_point_t p;
        lv_indev_get_point(lv_indev_active(), &p);
        int third = p.x * 3 / lv_display_get_horizontal_resolution(NULL);
        static const pet_item_t items[3] = {PET_ITEM_GUMMY, PET_ITEM_CRISPS, PET_ITEM_COFFEE};
        post((pet_event_t){.type = PET_INPUT_GIVE, .item = items[third < 0 ? 0 : third > 2 ? 2 : third]});
        return;
    }
    if (s_tap_pending) {  // second tap in the window
        cancel_tap();
        post((pet_event_t){.type = PET_INPUT_DOUBLE_TAP});
        return;
    }
    s_tap_pending = true;
    lv_timer_reset(s_tap_timer);
    lv_timer_resume(s_tap_timer);
}

// Swipe left or right to change his outfit.
static void on_gesture(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_active();
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) {
        return;
    }
    lv_indev_wait_release(indev);  // a swipe is not also a tap
    cancel_tap();
    post((pet_event_t){.type = PET_INPUT_SWIPE, .direction = dir == LV_DIR_LEFT ? 1 : -1});
}

static void on_long_press(lv_event_t *e)
{
    (void)e;
    cancel_tap();
    post((pet_event_t){.type = PET_INPUT_LONG_PRESS});
}

esp_err_t screen_start(QueueHandle_t events)
{
    s_events = events;

    lv_display_t *display = bsp_display_start();
    if (display == NULL) {
        ESP_LOGE(TAG, "display init failed");
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(bsp_display_backlight_on());

    bsp_display_lock(0);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    // SHORT_CLICKED is not sent after a long press, so the two don't overlap.
    lv_obj_add_event_cb(scr, on_tap, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(scr, on_long_press, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(scr, on_gesture, LV_EVENT_GESTURE, NULL);
    s_tap_timer = lv_timer_create(on_single_tap, DOUBLE_TAP_MS, NULL);
    lv_timer_pause(s_tap_timer);

    LV_DRAW_BUF_INIT_STATIC(s_canvas_buf);
    s_canvas = lv_canvas_create(scr);
    lv_canvas_set_draw_buf(s_canvas, &s_canvas_buf);
    lv_canvas_fill_bg(s_canvas, lv_color_black(), LV_OPA_COVER);

    // Nearest-neighbour upscale keeps the art pixels crisp.
    lv_image_set_scale(s_canvas, LV_SCALE_NONE * SCREEN_ART_SCALE);
    lv_image_set_antialias(s_canvas, false);
    lv_obj_center(s_canvas);
    // Let taps on the canvas fall through to the screen handler.
    lv_obj_remove_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);

    bsp_display_unlock();
    return ESP_OK;
}

// --- Drawing ----------------------------------------------------------------
// Each frame is layered from the generated sprites (see assets/sprites):
// background, body, face and props at the body's head offset, front layer.

typedef struct {
    sprite_bg_t bg;
    int fx;  // sprite_fx_t, or -1 for none
    sprite_body_t body;
    sprite_face_t face;
    int props[2];  // sprite_prop_t, -1 for none
    int front;     // sprite_front_t, -1 for none
    int dx, dy;    // whole-character offset
} scene_t;

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

// Map the pet's view to sprites.
static scene_t scene_for(const pet_view_t *v)
{
    static const sprite_bg_t bgs[PET_MOOD_COUNT] = {
        [PET_MOOD_ASLEEP] = SPRITE_BG_ASLEEP,   [PET_MOOD_GRUMPY] = SPRITE_BG_MORNING,
        [PET_MOOD_COMMUTE] = SPRITE_BG_COMMUTE, [PET_MOOD_DEADPAN] = SPRITE_BG_DEADPAN,
        [PET_MOOD_ME_TIME] = SPRITE_BG_BATH,    [PET_MOOD_NIGHT] = SPRITE_BG_NIGHT,
        [PET_MOOD_SPOOKY] = SPRITE_BG_SPOOKY,
    };
    static const sprite_face_t faces[PET_FACE_COUNT] = {
        [PET_FACE_BLANK] = SPRITE_FACE_BLANK,   [PET_FACE_GRUMPY] = SPRITE_FACE_GRUMPY,
        [PET_FACE_SMIRK] = SPRITE_FACE_SMIRK,   [PET_FACE_ANNOYED] = SPRITE_FACE_ANNOYED,
        [PET_FACE_DIZZY] = SPRITE_FACE_DIZZY,   [PET_FACE_HAPPY] = SPRITE_FACE_HAPPY,
        [PET_FACE_FANG] = SPRITE_FACE_FANG,     [PET_FACE_SLEEP] = SPRITE_FACE_SLEEP,
        [PET_FACE_SCREEN] = SPRITE_FACE_SCREEN, [PET_FACE_READ] = SPRITE_FACE_READ,
        [PET_FACE_SHADES] = SPRITE_FACE_SHADES,
    };
    static const int item_props[PET_ITEM_COUNT] = {
        [PET_ITEM_NONE] = -1, [PET_ITEM_GUMMY] = SPRITE_PROP_GUMMY,
        [PET_ITEM_CRISPS] = SPRITE_PROP_CRISPS, [PET_ITEM_COFFEE] = SPRITE_PROP_COFFEE,
    };

    scene_t s = {bgs[v->mood], -1, SPRITE_BODY_CASUAL, faces[v->face], {-1, -1}, -1, 0, 0};

    switch (v->outfit) {
    case PET_OUTFIT_COAT: s.body = v->back ? SPRITE_BODY_CASUAL_BACK : SPRITE_BODY_COAT; break;  // no coat back yet
    case PET_OUTFIT_PJS:
        s.body = v->back                           ? SPRITE_BODY_PJS_BACK
                 : v->device == PET_DEVICE_PHONE  ? SPRITE_BODY_PJS_PHONE
                 : v->device == PET_DEVICE_SWITCH ? SPRITE_BODY_PJS_SWITCH
                                                  : SPRITE_BODY_PJS;
        break;
    case PET_OUTFIT_BATH: s.body = SPRITE_BODY_BATH_BACK; break;
    case PET_OUTFIT_COSTUME: s.body = costume_body(v->costume, v->back); break;
    case PET_OUTFIT_LYING: s.body = v->scene == PET_SCENE_BEACH ? SPRITE_BODY_BEACH : SPRITE_BODY_COUCH; break;
    case PET_OUTFIT_HIDDEN: s.body = SPRITE_BODY_HEAD; break;
    default: s.body = v->back ? SPRITE_BODY_CASUAL_BACK : SPRITE_BODY_CASUAL; break;
    }

    switch (v->mood) {
    case PET_MOOD_ASLEEP: s.front = SPRITE_FRONT_BLANKET; s.dy = 6; break;
    case PET_MOOD_COMMUTE: s.fx = SPRITE_FX_TUNNEL; s.dy = 1; break;
    case PET_MOOD_SPOOKY: s.fx = SPRITE_FX_STARS; break;
    case PET_MOOD_ME_TIME:
        if (v->scene == PET_SCENE_COUCH) {
            s.bg = SPRITE_BG_COUCH;
        } else if (v->scene == PET_SCENE_BEACH) {
            s.bg = SPRITE_BG_BEACH;
            s.fx = SPRITE_FX_WAVES;
        } else if (!v->back) {
            s.front = SPRITE_FRONT_TUB;  // TODO: tub behind him when he stands up
            s.dy = 7;
        }
        break;
    default: break;
    }

    if (!v->back) {
        int n = 0;
        if (v->bedhead) s.props[n++] = SPRITE_PROP_BEDHEAD;
        if (v->item != PET_ITEM_NONE) s.props[n++] = item_props[v->item];
        else if (v->cup) s.props[n++] = SPRITE_PROP_CUP;
        else if (v->phone) s.props[n++] = SPRITE_PROP_PHONE;
    }
    if (v->shake) s.dx = 1;  // TODO: jiggle with the frame counter
    return s;
}

static void draw_scene(const scene_t *s, unsigned frame)
{
    uint16_t *buf = (uint16_t *)s_canvas_buf.data;
    int stride = s_canvas_buf.header.stride / sizeof(uint16_t);
#define DRAW(sprite, x, y) sprite_draw(buf, stride, SCREEN_ART_W, SCREEN_ART_H, (sprite), frame, (x), (y))

    DRAW(&sprite_bg[s->bg], 0, 0);
    if (s->fx >= 0) DRAW(&sprite_fx[s->fx], 0, 0);
    DRAW(&sprite_body[s->body], s->dx, s->dy);
    int hx = s->dx + sprite_head_offset[s->body].x, hy = s->dy + sprite_head_offset[s->body].y;
    DRAW(sprite_face(s->body, s->face), hx, hy);
    for (int i = 0; i < 2; i++) {
        if (s->props[i] >= 0) DRAW(sprite_prop(s->body, s->props[i]), hx, hy);
    }
    if (s->front >= 0) DRAW(&sprite_front[s->front], 0, 0);
#undef DRAW
}

// The snack picker: gummy, crisps and coffee across the bottom third.
static void draw_menu(void)
{
    uint16_t *buf = (uint16_t *)s_canvas_buf.data;
    int stride = s_canvas_buf.header.stride / sizeof(uint16_t);
    for (int y = 44; y < SCREEN_ART_H; y++) {
        for (int x = 0; x < SCREEN_ART_W; x++) {
            buf[y * stride + x] = 0x18e3;  // dark grey
        }
    }
    static const sprite_prop_t items[3] = {SPRITE_PROP_GUMMY, SPRITE_PROP_CRISPS, SPRITE_PROP_COFFEE};
    for (int i = 0; i < 3; i++) {
        const sprite_t *sp = sprite_prop(SPRITE_BODY_CASUAL, items[i]);
        if (sp == NULL) continue;
        const sprite_frame_t *f = &sp->frames[0];
        int cx = SCREEN_ART_W * (2 * i + 1) / 6, cy = 54;
        sprite_draw(buf, stride, SCREEN_ART_W, SCREEN_ART_H, sp, 0, cx - f->x - f->w / 2, cy - f->y - f->h / 2);
    }
}

void screen_draw(const pet_t *pet)
{
    pet_view_t v = pet_view(pet);
    if (v.line != NULL) {
        ESP_LOGI(TAG, "%s: \"%s\"", pet_mood_name(v.mood), v.line);  // no speech bubble yet
    }
    if (!bsp_display_lock(100)) {
        ESP_LOGW(TAG, "display busy, skipping frame");
        return;
    }
    s_menu_open = v.menu_open;
    scene_t s = scene_for(&v);
    draw_scene(&s, 0);  // TODO: tick animation frames, blinks and bobbing
    if (v.menu_open) {
        draw_menu();
    }
    lv_obj_invalidate(s_canvas);
    bsp_display_unlock();
}

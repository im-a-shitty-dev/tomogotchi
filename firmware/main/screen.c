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

static void on_tap(lv_event_t *e)
{
    (void)e;
    pet_event_t ev = PET_EVENT_POKE;
    xQueueSend(s_events, &ev, 0);
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
    lv_obj_add_event_cb(scr, on_tap, LV_EVENT_CLICKED, NULL);

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

static bool is_cute(sprite_body_t body)
{
    return body == SPRITE_BODY_CHIIKAWA || body == SPRITE_BODY_PIKACHU || body == SPRITE_BODY_SYLVEON ||
           body == SPRITE_BODY_TREX || body == SPRITE_BODY_TRIKE;
}

static scene_t scene_for(const pet_t *pet)
{
    scene_t s = {SPRITE_BG_DEADPAN, -1, SPRITE_BODY_CASUAL, SPRITE_FACE_BLANK, {-1, -1}, -1, 0, 0};
    switch (pet->mood) {
    case PET_MOOD_GRUMPY:
        s.bg = SPRITE_BG_MORNING;
        s.face = SPRITE_FACE_GRUMPY;
        s.props[0] = SPRITE_PROP_BEDHEAD;
        s.props[1] = SPRITE_PROP_CUP;
        break;
    case PET_MOOD_SPOOKY:
        // Each poke swaps the costume, as in the mockup.
        s.bg = SPRITE_BG_SPOOKY;
        s.fx = SPRITE_FX_STARS;
        s.body = SPRITE_BODY_DRACULA + pet->pokes % (SPRITE_BODY_SYLVEON - SPRITE_BODY_DRACULA + 1);
        break;
    case PET_MOOD_ME_TIME:
        s.bg = SPRITE_BG_BATH;
        s.body = SPRITE_BODY_HEAD;
        s.face = SPRITE_FACE_HAPPY;
        s.front = SPRITE_FRONT_TUB;
        s.dy = 7;
        break;
    default:
        s.props[0] = SPRITE_PROP_PHONE;
        break;
    }
    switch (pet->reaction) {
    case PET_REACTION_SMIRK:
        s.face = pet->mood == PET_MOOD_SPOOKY && !is_cute(s.body) ? SPRITE_FACE_FANG : SPRITE_FACE_SMIRK;
        break;
    case PET_REACTION_ANNOYED:
        s.face = SPRITE_FACE_ANNOYED;
        break;
    case PET_REACTION_DIZZY:
        s.face = SPRITE_FACE_DIZZY;
        break;
    default:
        break;
    }
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

void screen_draw(const pet_t *pet)
{
    if (!bsp_display_lock(100)) {
        ESP_LOGW(TAG, "display busy, skipping frame");
        return;
    }
    scene_t s = scene_for(pet);
    draw_scene(&s, 0);  // TODO: tick animation frames, blinks and bobbing
    lv_obj_invalidate(s_canvas);
    bsp_display_unlock();
}

#include "screen.h"

#include <string.h>

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"
#include "scene.h"
#include "sprites_gen.h"

static const char *TAG = "screen";

static lv_obj_t *s_canvas;
static QueueHandle_t s_events;
static const char *s_last_line;  // to log each speech line once

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
// The scene component (components/scene) decides what to draw for the pet's
// view at each animation tick and renders it into the canvas buffer. We then
// compare with the previous frame and only invalidate the box that changed,
// so a blink redraws a few art pixels instead of the whole panel.

static uint16_t s_prev[SCREEN_ART_W * SCREEN_ART_H];
static bool s_prev_valid;

// Mark art pixels `r` for redraw, in screen coordinates.
static void invalidate_art(const scene_rect_t *r)
{
    lv_area_t c;
    lv_obj_get_coords(s_canvas, &c);
    // The canvas is scaled about its centre.
    int ox = (c.x1 + c.x2 + 1) / 2 - SCREEN_ART_W * SCREEN_ART_SCALE / 2;
    int oy = (c.y1 + c.y2 + 1) / 2 - SCREEN_ART_H * SCREEN_ART_SCALE / 2;
    lv_area_t a = {
        .x1 = ox + r->x * SCREEN_ART_SCALE,
        .y1 = oy + r->y * SCREEN_ART_SCALE,
        .x2 = ox + (r->x + r->w) * SCREEN_ART_SCALE - 1,
        .y2 = oy + (r->y + r->h) * SCREEN_ART_SCALE - 1,
    };
    lv_inv_area(lv_obj_get_display(s_canvas), &a);
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

void screen_draw(const pet_t *pet, uint32_t tick)
{
    pet_view_t v = pet_view(pet);
    if (v.line != NULL && v.line != s_last_line) {
        ESP_LOGI(TAG, "%s: \"%s\"", pet_mood_name(v.mood), v.line);  // no speech bubble yet
    }
    s_last_line = v.line;
    if (!bsp_display_lock(100)) {
        ESP_LOGW(TAG, "display busy, skipping frame");
        return;
    }
    s_menu_open = v.menu_open;

    uint16_t *buf = (uint16_t *)s_canvas_buf.data;
    int stride = s_canvas_buf.header.stride / sizeof(uint16_t);
    scene_t scene;
    scene_build(&scene, &v, tick);
    scene_render(&scene, buf, stride);
    if (v.menu_open) {
        draw_menu();
    }

    scene_rect_t r = {0, 0, SCREEN_ART_W, SCREEN_ART_H};
    if (!s_prev_valid || scene_diff(s_prev, SCREEN_ART_W, buf, stride, &r)) {
        invalidate_art(&r);
        for (int y = 0; y < SCREEN_ART_H; y++) {
            memcpy(&s_prev[y * SCREEN_ART_W], &buf[y * stride], SCREEN_ART_W * sizeof(uint16_t));
        }
        s_prev_valid = true;
    }
    bsp_display_unlock();
}

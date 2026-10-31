#include "screen.h"

#include <string.h>
#include <time.h>

#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "scene.h"
#include "sprites_gen.h"

static const char *TAG = "screen";

static lv_obj_t *s_canvas;
static QueueHandle_t s_events;
static const char *s_last_line;  // to log each speech line once
static scene_hud_t s_hud;          // pop-up hearts and the peek strip

// RGB565 canvas buffer: 52 * 64 * 2 bytes, about 6.6 KB.
LV_DRAW_BUF_DEFINE_STATIC(s_canvas_buf, SCREEN_ART_W, SCREEN_ART_H, LV_COLOR_FORMAT_RGB565);

// --- Input ------------------------------------------------------------------
// A tap waits briefly in case a second one makes it a double-tap (turn him
// around). A long press opens the snack picker; while it is open, a tap on
// the picker strip at the bottom picks the item under your finger (left,
// middle or right third), and a tap anywhere else closes it. Swiping left or
// right changes his outfit; swiping up changes the scene; swiping down peeks
// at the clock and his hearts.

// From the first tap's release to the second's. 280 ms was too tight: a
// missed double-tap became two taps, which annoyed him.
#define DOUBLE_TAP_MS 400
#define MENU_TOP_ART_Y 44  // the snack picker strip's top row, in art pixels

static lv_timer_t *s_tap_timer;
static bool s_tap_pending;  // a tap is waiting to see if a second one follows
static volatile bool s_menu_open;
static volatile bool s_peek_asked;  // swiped down; screen_draw opens the strip

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
        lv_area_t c;
        lv_obj_get_coords(s_canvas, &c);
        int art_top = (c.y1 + c.y2 + 1) / 2 - SCREEN_ART_H * SCREEN_ART_SCALE / 2;  // scaled about its centre
        if (p.y < art_top + MENU_TOP_ART_Y * SCREEN_ART_SCALE) {
            post((pet_event_t){.type = PET_INPUT_TAP});  // tapped away: the pet closes it
            return;
        }
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

// Swipe left or right to change his outfit, up to change the scene, or down to peek at the clock
// and his hearts. The peek is display-only, so it doesn't go to the pet.
static void on_gesture(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_active();
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT && dir != LV_DIR_TOP && dir != LV_DIR_BOTTOM) {
        return;
    }
    lv_indev_wait_release(indev);  // a swipe is not also a tap
    cancel_tap();
    if (dir == LV_DIR_BOTTOM) {
        s_peek_asked = true;
        return;
    }
    if (dir == LV_DIR_TOP) {  // swipe up: next scene
        post((pet_event_t){.type = PET_INPUT_SWIPE_UP});
        return;
    }
    post((pet_event_t){.type = PET_INPUT_SWIPE, .direction = dir == LV_DIR_LEFT ? 1 : -1});
}

static void on_long_press(lv_event_t *e)
{
    (void)e;
    cancel_tap();
    post((pet_event_t){.type = PET_INPUT_LONG_PRESS});
}

// --- Display start-up -------------------------------------------------------
// This is bsp_display_start() done by hand, for two reasons. We keep the panel
// IO handle, to send the panel to sleep (the BSP only offers brightness). And
// LVGL polls the touch chip instead of using its interrupt line, so the line
// is free for power.c to wake the board with a tap.

static esp_lcd_panel_io_handle_t s_io;
static lv_indev_t *s_touch;
static TaskHandle_t s_lvgl_task;

// The panels (SH8601 or CO5300) want even-aligned areas.
static void round_area(lv_event_t *e)
{
    lv_area_t *a = lv_event_get_param(e);
    a->x1 &= ~1;
    a->y1 &= ~1;
    a->x2 |= 1;
    a->y2 |= 1;
}

static lv_display_t *display_start(void)
{
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    if (lvgl_port_init(&port_cfg) != ESP_OK) {
        return NULL;
    }
    s_lvgl_task = xTaskGetHandle("taskLVGL");

    esp_lcd_panel_handle_t panel;
    if (bsp_display_new(NULL, &panel, &s_io) != ESP_OK) {
        return NULL;
    }
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = s_io,
        .panel_handle = panel,
        .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = BSP_LCD_DRAW_BUFF_DOUBLE,
        .hres = BSP_LCD_H_RES,
        .vres = BSP_LCD_V_RES,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {.sw_rotate = true, .swap_bytes = true},
    };
    lv_display_t *display = lvgl_port_add_disp(&disp_cfg);
    if (display == NULL) {
        return NULL;
    }
    lv_display_add_event_cb(display, round_area, LV_EVENT_INVALIDATE_AREA, NULL);

    esp_lcd_touch_handle_t tp;
    if (bsp_touch_new(NULL, &tp) == ESP_OK) {
        tp->config.int_gpio_num = GPIO_NUM_NC;  // poll; power.c owns the line
        const lvgl_port_touch_cfg_t touch_cfg = {.disp = display, .handle = tp};
        s_touch = lvgl_port_add_touch(&touch_cfg);
    }
    if (s_touch == NULL) {
        ESP_LOGW(TAG, "touch unavailable");
    }
    bsp_display_brightness_set(100);
    return display;
}

esp_err_t screen_start(QueueHandle_t events)
{
    s_events = events;

    lv_display_t *display = display_start();
    if (display == NULL) {
        ESP_LOGE(TAG, "display init failed");
        return ESP_FAIL;
    }

    bsp_display_lock(0);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    // SHORT_CLICKED is not sent after a long press, so the two don't overlap.
    lv_obj_add_event_cb(scr, on_tap, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(scr, on_long_press, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_event_cb(scr, on_gesture, LV_EVENT_GESTURE, NULL);
    s_tap_timer = lv_timer_create(on_single_tap, DOUBLE_TAP_MS, NULL);
    lv_timer_pause(s_tap_timer);

    scene_hud_init(&s_hud);
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

// --- Screen sleep -------------------------------------------------------------
// Off: brightness to zero, panel to sleep, LVGL stopped so nothing wakes the
// chip. The LVGL task is suspended because with its timers off it would spin
// every millisecond and keep the chip out of light sleep.

#define LCD_CMD(c) ((0x02u << 24) | ((uint32_t)(c) << 8))  // QSPI write, as the BSP does
#define LCD_SLPIN 0x10
#define LCD_SLPOUT 0x11
#define LCD_DISPOFF 0x28
#define LCD_DISPON 0x29

static bool s_on = true;
static bool s_prev_valid;

static void lcd_cmd(uint8_t cmd)
{
    esp_err_t err = esp_lcd_panel_io_tx_param(s_io, LCD_CMD(cmd), NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "panel cmd 0x%02x: %s", cmd, esp_err_to_name(err));
    }
}

void screen_set_on(bool on)
{
    if (on == s_on) {
        return;
    }
    s_on = on;
    if (!on) {
        bsp_display_lock(0);
        cancel_tap();
        bsp_display_brightness_set(0);
        lcd_cmd(LCD_DISPOFF);
        lcd_cmd(LCD_SLPIN);
        lvgl_port_stop();
        if (s_lvgl_task) {
            vTaskSuspend(s_lvgl_task);  // while we hold the lock, so not mid-frame
        }
        bsp_display_unlock();
        ESP_LOGI(TAG, "screen off");
        return;
    }

    lcd_cmd(LCD_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));  // panel needs this after sleep-out
    lcd_cmd(LCD_DISPON);
    if (s_lvgl_task) {
        vTaskResume(s_lvgl_task);
    }
    lvgl_port_resume();
    bsp_display_lock(0);
    s_prev_valid = false;  // send the whole frame again
    lv_obj_invalidate(lv_screen_active());
    if (s_touch) {
        lv_indev_wait_release(s_touch);  // the waking tap isn't also a poke
    }
    bsp_display_unlock();
    bsp_display_brightness_set(100);
    ESP_LOGI(TAG, "screen on");
}

// --- Drawing ----------------------------------------------------------------
// The scene component (components/scene) decides what to draw for the pet's
// view at each animation tick and renders it into the canvas buffer. We then
// compare with the previous frame and only invalidate the box that changed,
// so a blink redraws a few art pixels instead of the whole panel.

static uint16_t s_prev[SCREEN_ART_W * SCREEN_ART_H];

// Local time for the peek clock, or hour -1 until SNTP or the RTC has set it.
static scene_time_t clock_now(void)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    if (tm.tm_year + 1900 < 2024) {
        return (scene_time_t){.hour = -1};
    }
    return (scene_time_t){.hour = tm.tm_hour, .minute = tm.tm_min, .second = tm.tm_sec};
}

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
    // Invalidate on the screen, not the canvas: the canvas object's own box is
    // the unscaled 52x64, which would clip the area.
    lv_obj_invalidate_area(lv_obj_get_screen(s_canvas), &a);
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
        ESP_LOGI(TAG, "%s: \"%s\"", pet_mood_name(v.mood), v.line);  // also in the bubble (scene)
    }
    s_last_line = v.line;
    if (!s_on) {
        return;
    }
    if (!bsp_display_lock(100)) {
        ESP_LOGW(TAG, "display busy, skipping frame");
        return;
    }
    s_menu_open = v.menu_open;

    uint16_t *buf = (uint16_t *)s_canvas_buf.data;
    int stride = s_canvas_buf.header.stride / sizeof(uint16_t);
    if (s_peek_asked) {
        s_peek_asked = false;
        scene_hud_peek(&s_hud, tick);
    }
    scene_time_t now = clock_now();
    scene_t scene;
    scene_build(&scene, &v, tick);
    scene_hud(&scene, &s_hud, &v, &now, tick);
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

#include "screen.h"

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

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

// --- Placeholder face -------------------------------------------------------
// Stand-in art until real sprites arrive from the character design work.

static void px(int x, int y, lv_color_t c)
{
    if (x >= 0 && x < SCREEN_ART_W && y >= 0 && y < SCREEN_ART_H) {
        lv_canvas_set_px(s_canvas, x, y, c, LV_OPA_COVER);
    }
}

static void rect(int x, int y, int w, int h, lv_color_t c)
{
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            px(x + i, y + j, c);
        }
    }
}

static void draw_face(const pet_t *pet)
{
    const lv_color_t skin = lv_color_hex(0xF2D3B3);
    const lv_color_t hair = lv_color_hex(0x1A1A22);
    const lv_color_t ink = lv_color_hex(0x2B1E1E);
    const lv_color_t blush = lv_color_hex(0xE88A8A);

    // Head and mullet.
    rect(12, 14, 28, 28, skin);
    rect(10, 10, 32, 8, hair);
    rect(10, 18, 4, 26, hair);
    rect(38, 18, 4, 26, hair);

    // Eyes: half-lidded when grumpy or annoyed, flat lines otherwise.
    bool lidded = pet->mood == PET_MOOD_GRUMPY || pet->reaction == PET_REACTION_ANNOYED;
    if (pet->reaction == PET_REACTION_DIZZY) {
        rect(17, 24, 1, 1, ink); rect(19, 24, 1, 1, ink); rect(18, 25, 1, 1, ink);
        rect(17, 26, 1, 1, ink); rect(19, 26, 1, 1, ink);
        rect(32, 24, 1, 1, ink); rect(34, 24, 1, 1, ink); rect(33, 25, 1, 1, ink);
        rect(32, 26, 1, 1, ink); rect(34, 26, 1, 1, ink);
    } else {
        rect(17, 25, 4, lidded ? 1 : 2, ink);
        rect(31, 25, 4, lidded ? 1 : 2, ink);
    }

    // Mouth.
    switch (pet->reaction) {
    case PET_REACTION_SMIRK:
        rect(24, 34, 5, 1, ink);
        px(29, 33, ink);
        rect(15, 30, 3, 1, blush);
        rect(34, 30, 3, 1, blush);
        break;
    case PET_REACTION_ANNOYED:
        rect(23, 35, 6, 1, ink);
        px(22, 36, ink);
        px(29, 36, ink);
        break;
    case PET_REACTION_DIZZY:
        rect(24, 34, 4, 3, ink);
        break;
    default:
        rect(24, 35, 4, 1, ink);
        break;
    }
}

void screen_draw(const pet_t *pet)
{
    if (!bsp_display_lock(100)) {
        ESP_LOGW(TAG, "display busy, skipping frame");
        return;
    }
    lv_canvas_fill_bg(s_canvas, lv_color_black(), LV_OPA_COVER);
    draw_face(pet);
    lv_obj_invalidate(s_canvas);
    bsp_display_unlock();
}

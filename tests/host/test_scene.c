#include <stdio.h>
#include <string.h>

#include "scene.h"

static int failures = 0;

#define CHECK(cond)                                                   \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,    \
                    __LINE__, #cond);                                 \
            failures++;                                               \
        }                                                             \
    } while (0)

static pet_view_t view(pet_mood_t mood)
{
    pet_view_t v;
    memset(&v, 0, sizeof v);
    v.mood = mood;
    v.face = PET_FACE_BLANK;
    return v;
}

// Index of the first layer drawing `sprite`, or -1.
static int find(const scene_t *s, const sprite_t *sprite)
{
    for (int i = 0; i < s->count; i++) {
        if (s->layers[i].sprite == sprite) return i;
    }
    return -1;
}

static const scene_layer_t *body_layer(const scene_t *s, sprite_body_t body)
{
    int i = find(s, &sprite_body[body]);
    return i < 0 ? NULL : &s->layers[i];
}

static void test_resting_faces_blink(void)
{
    scene_t s;
    pet_view_t v = view(PET_MOOD_DEADPAN);
    scene_build(&s, &v, 0);
    CHECK(find(&s, sprite_face(SPRITE_BODY_CASUAL, SPRITE_FACE_BLINK)) >= 0);
    scene_build(&s, &v, 5);
    CHECK(find(&s, sprite_face(SPRITE_BODY_CASUAL, SPRITE_FACE_BLANK)) >= 0);
    CHECK(find(&s, sprite_face(SPRITE_BODY_CASUAL, SPRITE_FACE_BLINK)) < 0);

    v = view(PET_MOOD_GRUMPY);
    v.face = PET_FACE_GRUMPY;
    scene_build(&s, &v, 32);
    CHECK(find(&s, sprite_face(SPRITE_BODY_CASUAL, SPRITE_FACE_GRUMPYBLINK)) >= 0);

    // Reactions hold their face.
    v.face = PET_FACE_SMIRK;
    scene_build(&s, &v, 0);
    CHECK(find(&s, sprite_face(SPRITE_BODY_CASUAL, SPRITE_FACE_SMIRK)) >= 0);
}

static void test_idle_bob(void)
{
    scene_t s;
    pet_view_t v = view(PET_MOOD_DEADPAN);
    scene_build(&s, &v, 0);
    CHECK(body_layer(&s, SPRITE_BODY_CASUAL)->y == 0);
    scene_build(&s, &v, 5);
    CHECK(body_layer(&s, SPRITE_BODY_CASUAL)->y == 1);
    scene_build(&s, &v, 10);
    CHECK(body_layer(&s, SPRITE_BODY_CASUAL)->y == 0);
}

static void test_shake_jiggles(void)
{
    scene_t s;
    pet_view_t v = view(PET_MOOD_DEADPAN);
    v.shake = true;
    scene_build(&s, &v, 0);
    int a = body_layer(&s, SPRITE_BODY_CASUAL)->x;
    scene_build(&s, &v, 1);
    int b = body_layer(&s, SPRITE_BODY_CASUAL)->x;
    CHECK(a == -1 && b == 1);
}

static void test_tub_in_front_or_behind(void)
{
    scene_t s;
    pet_view_t v = view(PET_MOOD_ME_TIME);
    v.scene = PET_SCENE_BATH;
    v.outfit = PET_OUTFIT_HIDDEN;
    v.face = PET_FACE_HAPPY;
    scene_build(&s, &v, 0);
    CHECK(find(&s, &sprite_front[SPRITE_FRONT_TUB]) > find(&s, &sprite_body[SPRITE_BODY_HEAD]));
    CHECK(find(&s, &sprite_fx[SPRITE_FX_STEAM]) >= 0);
    CHECK(find(&s, &sprite_fx[SPRITE_FX_FOAM]) == find(&s, &sprite_front[SPRITE_FRONT_TUB]) + 1);
    CHECK(body_layer(&s, SPRITE_BODY_HEAD)->y == 7);

    // Stood up with his back to you: tub behind, no steam, no face.
    v.outfit = PET_OUTFIT_BATH;
    v.back = true;
    scene_build(&s, &v, 0);
    int tub = find(&s, &sprite_front[SPRITE_FRONT_TUB]);
    CHECK(tub >= 0 && tub < find(&s, &sprite_body[SPRITE_BODY_BATH_BACK]));
    CHECK(find(&s, &sprite_fx[SPRITE_FX_FOAM]) == tub + 1);
    CHECK(find(&s, &sprite_fx[SPRITE_FX_STEAM]) < 0);
    CHECK(find(&s, sprite_face(SPRITE_BODY_BATH_BACK, SPRITE_FACE_HAPPY)) < 0);

    // Shaken in the tub: splash instead of steam.
    v = view(PET_MOOD_ME_TIME);
    v.scene = PET_SCENE_BATH;
    v.outfit = PET_OUTFIT_HIDDEN;
    v.shake = true;
    scene_build(&s, &v, 0);
    CHECK(find(&s, &sprite_fx[SPRITE_FX_SPLASH]) >= 0);
    CHECK(find(&s, &sprite_fx[SPRITE_FX_STEAM]) < 0);
}

static void test_fx_follow_the_tick(void)
{
    scene_t s;
    pet_view_t v = view(PET_MOOD_ME_TIME);
    v.scene = PET_SCENE_BEACH;
    v.outfit = PET_OUTFIT_LYING;
    scene_build(&s, &v, 7);
    int i = find(&s, &sprite_fx[SPRITE_FX_WAVES]);
    CHECK(i >= 0 && s.layers[i].frame == 2);  // sampled every 3 ticks

    v = view(PET_MOOD_ASLEEP);
    v.outfit = PET_OUTFIT_HIDDEN;
    v.face = PET_FACE_SLEEP;
    scene_build(&s, &v, 3);
    i = find(&s, &sprite_fx[SPRITE_FX_ZZZ]);
    CHECK(i == s.count - 1 && s.layers[i].frame == 3);  // Zs float over the blanket
    v.line = "zzz…";
    scene_build(&s, &v, 3);
    CHECK(find(&s, &sprite_fx[SPRITE_FX_ZZZ]) < 0);  // not while he's talking

    v = view(PET_MOOD_SPOOKY);
    v.outfit = PET_OUTFIT_COSTUME;
    v.costume = PET_COSTUME_DRACULA;
    scene_build(&s, &v, 100);
    i = find(&s, &sprite_fx[SPRITE_FX_BAT]);
    CHECK(i >= 0 && s.layers[i].frame == 100 % sprite_fx[SPRITE_FX_BAT].count);
    CHECK(s.layers[i + 1].sprite == &sprite_fx[SPRITE_FX_BAT]);  // and a second bat
}

static void test_commute_lights_flicker(void)
{
    static uint16_t lit[SCENE_W * SCENE_H], dimmed[SCENE_W * SCENE_H];
    scene_t s;
    pet_view_t v = view(PET_MOOD_COMMUTE);
    v.outfit = PET_OUTFIT_COAT;
    scene_build(&s, &v, 150);
    CHECK(s.dim);
    scene_render(&s, dimmed, SCENE_W);
    s.dim = false;
    scene_render(&s, lit, SCENE_W);
    int darker = 0;
    for (int i = 0; i < SCENE_W * SCENE_H; i++) {
        darker += (dimmed[i] >> 11) < (lit[i] >> 11);
    }
    CHECK(darker > SCENE_W * SCENE_H / 2);
    scene_build(&s, &v, 2);
    CHECK(!s.dim);
}

static void test_every_view_builds(void)
{
    // Every mood, outfit and flag combination stays within the layer budget
    // and never pushes a NULL sprite.
    scene_t s;
    for (int mood = 0; mood < PET_MOOD_COUNT; mood++)
        for (int outfit = 0; outfit < PET_OUTFIT_COUNT; outfit++)
            for (int scene = 0; scene < PET_SCENE_COUNT; scene++)
                for (int flags = 0; flags < 32; flags++) {
                    pet_view_t v = view(mood);
                    v.outfit = outfit;
                    v.scene = scene;
                    v.costume = PET_COSTUME_SYLVEON;
                    v.device = PET_DEVICE_SWITCH;
                    v.back = flags & 1;
                    v.shake = flags & 2;
                    v.bedhead = flags & 4;
                    v.item = flags & 8 ? PET_ITEM_CRISPS : PET_ITEM_NONE;
                    v.phone = flags & 16;
                    for (uint32_t t = 0; t < 3; t++) {
                        scene_build(&s, &v, t);
                        CHECK(s.count > 1 && s.count <= SCENE_MAX_LAYERS);
                        for (int i = 0; i < s.count; i++) CHECK(s.layers[i].sprite != NULL);
                    }
                }
}

static void test_diff_finds_the_changed_box(void)
{
    static uint16_t a[SCENE_W * SCENE_H], b[SCENE_W * SCENE_H];
    scene_rect_t r;
    memset(a, 0, sizeof a);
    memset(b, 0, sizeof b);
    CHECK(!scene_diff(a, SCENE_W, b, SCENE_W, &r));
    b[10 * SCENE_W + 3] = 1;
    CHECK(scene_diff(a, SCENE_W, b, SCENE_W, &r));
    CHECK(r.x == 3 && r.y == 10 && r.w == 1 && r.h == 1);
    b[20 * SCENE_W + 40] = 1;
    CHECK(scene_diff(a, SCENE_W, b, SCENE_W, &r));
    CHECK(r.x == 3 && r.y == 10 && r.w == 38 && r.h == 11);
}

static void test_blink_redraws_only_the_eyes(void)
{
    // The point of the diff: a blink touches a small box, not the screen.
    static uint16_t open[SCENE_W * SCENE_H], shut[SCENE_W * SCENE_H];
    scene_t s;
    pet_view_t v = view(PET_MOOD_DEADPAN);
    v.phone = true;
    scene_build(&s, &v, 31);  // eyes open, bob down... next tick bob is the same
    scene_render(&s, open, SCENE_W);
    scene_build(&s, &v, 32);
    scene_render(&s, shut, SCENE_W);
    scene_rect_t r;
    CHECK(scene_diff(open, SCENE_W, shut, SCENE_W, &r));
    CHECK(r.w * r.h < 40);
}

int main(void)
{
    test_resting_faces_blink();
    test_idle_bob();
    test_shake_jiggles();
    test_tub_in_front_or_behind();
    test_fx_follow_the_tick();
    test_commute_lights_flicker();
    test_every_view_builds();
    test_diff_finds_the_changed_box();
    test_blink_redraws_only_the_eyes();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all scene tests passed\n");
    return 0;
}

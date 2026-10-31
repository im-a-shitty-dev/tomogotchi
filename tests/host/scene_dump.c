// Dump animated frames of a few scenes as raw RGB565, for previews:
//   build/host/scene_dump TICKS > frames.bin   (then tools/sprites/animate.py)
// Writes, for each scene in order, TICKS frames of 52x64 little-endian pixels.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scene.h"

static pet_view_t base(pet_mood_t mood, pet_outfit_t outfit, pet_face_t face)
{
    pet_view_t v;
    memset(&v, 0, sizeof v);
    v.mood = mood;
    v.outfit = outfit;
    v.face = face;
    v.happiness = PET_HAPPINESS_START;  // no rain cloud or sparkles
    return v;
}

int main(int argc, char **argv)
{
    int ticks = argc > 1 ? atoi(argv[1]) : 66;
    pet_view_t views[12];
    int n = 0;
    views[n] = base(PET_MOOD_DEADPAN, PET_OUTFIT_CASUAL, PET_FACE_BLANK), views[n++].phone = true;
    views[n] = base(PET_MOOD_GRUMPY, PET_OUTFIT_CASUAL, PET_FACE_GRUMPY), views[n].cup = views[n].bedhead = true, n++;
    views[n] = base(PET_MOOD_COMMUTE, PET_OUTFIT_COAT, PET_FACE_BLANK), views[n++].phone = true;
    views[n] = base(PET_MOOD_ME_TIME, PET_OUTFIT_HIDDEN, PET_FACE_HAPPY), views[n++].scene = PET_SCENE_BATH;
    views[n] = base(PET_MOOD_ME_TIME, PET_OUTFIT_BATH, PET_FACE_HAPPY), views[n].scene = PET_SCENE_BATH, views[n++].back = true;
    views[n] = base(PET_MOOD_ME_TIME, PET_OUTFIT_LYING, PET_FACE_READ), views[n++].scene = PET_SCENE_COUCH;
    views[n] = base(PET_MOOD_ME_TIME, PET_OUTFIT_LYING, PET_FACE_SHADES), views[n++].scene = PET_SCENE_BEACH;
    views[n] = base(PET_MOOD_NIGHT, PET_OUTFIT_PJS, PET_FACE_SCREEN), views[n++].device = PET_DEVICE_SWITCH;
    views[n++] = base(PET_MOOD_ASLEEP, PET_OUTFIT_HIDDEN, PET_FACE_SLEEP);
    views[n] = base(PET_MOOD_SPOOKY, PET_OUTFIT_COSTUME, PET_FACE_BLANK), views[n++].costume = PET_COSTUME_DRACULA;
    views[n] = base(PET_MOOD_DEADPAN, PET_OUTFIT_CASUAL, PET_FACE_DIZZY), views[n++].shake = true;
    views[n] = base(PET_MOOD_ME_TIME, PET_OUTFIT_HIDDEN, PET_FACE_ANNOYED), views[n].scene = PET_SCENE_BATH, views[n++].shake = true;

    static uint16_t buf[SCENE_W * SCENE_H];
    for (int i = 0; i < n; i++) {
        for (int t = 0; t < ticks; t++) {
            scene_t s;
            scene_build(&s, &views[i], (uint32_t)t);
            scene_render(&s, buf, SCENE_W);
            fwrite(buf, sizeof buf, 1, stdout);
        }
    }
    return 0;
}

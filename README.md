# tomogotchi

A tamagotchi-style toy for the Waveshare 1.8" touch AMOLED board, in either its
[ESP32-C6](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-1.8) or
[ESP32-S3](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.8) version. A little pixel-art Tomo lives on the screen and reacts to taps and shakes.

## Layout

```
firmware/                ESP-IDF project
  main/                  app: entry point, screen, motion sensing
  components/pet/        pet logic (plain C, no hardware calls)
  components/sprites/    sprite drawing + generated sprite data
  components/scene/      what to draw for the pet at each animation tick
assets/sprites/          the pixel art, as PNGs (see assets/sprites/README.md)
tools/sprites/           PNG -> C converter, preview, mockup seeding
tests/host/              unit tests for the pet logic, sprites and scenes, run on your computer
docs/adr/                architecture decisions
```

## Build and flash

Needs [ESP-IDF v5.5+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/linux-macos-setup.html).

The board comes with an ESP32-C6 or an ESP32-S3 chip. Pick the target that
matches yours; each gets its own build folder (and its own `sdkconfig` inside it):

```sh
. ~/esp/esp-idf/export.sh        # wherever ESP-IDF is installed
cd firmware
# ESP32-C6 board
idf.py -B build-esp32c6 -DIDF_TARGET=esp32c6 -p /dev/ttyACM0 flash monitor
# ESP32-S3 board
idf.py -B build-esp32s3 -DIDF_TARGET=esp32s3 -p /dev/ttyACM0 flash monitor
```

Per-chip settings live in `sdkconfig.defaults.esp32c6` / `.esp32s3`. After
changing any `sdkconfig.defaults*` file, delete that build folder so it's
picked up.

The first build downloads the Waveshare board package and LVGL into
`firmware/managed_components/`.

## Host tests

```sh
cmake -S tests/host -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

## Sprites

After changing anything in `assets/sprites/`, regenerate the C:

```sh
pip install pillow
python tools/sprites/build.py
python tools/sprites/preview.py   # optional: contact sheet in build/sprites-preview.png
python tools/sprites/animate.py   # optional: animated GIF from the real scene code
                                  # (build the host tests first)
```

CI runs the host tests, checks the sprite C is up to date, and builds the
firmware on every push.

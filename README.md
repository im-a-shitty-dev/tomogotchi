# tomogotchi

A tamagotchi-style toy for the Waveshare
[ESP32-C6 Touch AMOLED 1.8](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-1.8)
board. A little pixel-art Tomo lives on the screen and reacts to taps and shakes.

## Layout

```
firmware/                ESP-IDF project
  main/                  app: entry point, screen, motion sensing
  components/pet/        pet logic (plain C, no hardware calls)
  components/sprites/    sprite drawing + generated sprite data
assets/sprites/          the pixel art, as PNGs (see assets/sprites/README.md)
tools/sprites/           PNG -> C converter, preview, mockup seeding
tests/host/              unit tests for the pet logic and sprites, run on your computer
docs/adr/                architecture decisions
```

## Build and flash

Needs [ESP-IDF v5.5+](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c6/get-started/linux-macos-setup.html).

```sh
. ~/esp/esp-idf/export.sh        # wherever ESP-IDF is installed
cd firmware
idf.py set-target esp32c6        # first time only
idf.py -p /dev/ttyACM0 build flash monitor
```

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
```

CI runs the host tests, checks the sprite C is up to date, and builds the
firmware on every push.

# tomogotchi

A tamagotchi-style toy for the Waveshare
[ESP32-C6 Touch AMOLED 1.8](https://github.com/waveshareteam/ESP32-C6-Touch-AMOLED-1.8)
board. A little pixel-art Tomo lives on the screen and reacts to taps and shakes.

## Layout

```
firmware/                ESP-IDF project
  main/                  app: entry point, screen, motion sensing
  components/pet/        pet logic (plain C, no hardware calls)
tests/host/              unit tests for the pet logic, run on your computer
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

CI runs both the host tests and a firmware build on every push.

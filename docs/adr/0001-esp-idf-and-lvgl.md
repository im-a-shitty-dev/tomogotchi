# 1. ESP-IDF + LVGL in C for the firmware

Date: 2026-10-31
Status: accepted

## Context

The target is the Waveshare ESP32-C6 Touch AMOLED 1.8 board: ESP32-C6, ~512 KB
SRAM, **no PSRAM**, 368x448 AMOLED (SH8601 over QSPI), FT3168 touch, QMI8658
IMU, AXP2101 power chip, PCF85063 RTC.

One full RGB565 frame is ~330 KB, so the screen must be drawn in strips.

Options considered:

- **ESP-IDF (C)**: Espressif's SDK. Waveshare publishes an ESP-IDF board
  package (`waveshare/esp32_c6_touch_amoled_1_8`) with screen, touch, I2C and
  LVGL already wired up, plus a QMI8658 driver.
- **Arduino (C++)**: easier start, thinner C6 support, harder to grow.
- **Rust (esp-hal)**: good C6 support but no drivers for this screen/touch.
- **MicroPython**: closest to Python, but no PSRAM and no driver for this
  screen; drawing would be slow.

## Decision

ESP-IDF v5.5+ with Waveshare's board package and LVGL 9.

The character is pixel art on a 52x64 grid. We draw it into a small (6.6 KB)
LVGL canvas and let LVGL scale it 7x to the panel with nearest-neighbour, so
we never hold a full frame in RAM.

Pet logic (`firmware/components/pet`) is plain C with no hardware calls, so it
is unit-tested on the host (`tests/host`) and could back a desktop simulator.

## Consequences

- Hardware bring-up is mostly done by the board package.
- Code is C, not Python. The hardware-free pet logic is kept small and tested
  to make changes safe.
- If software scaling turns out too slow, we can switch to pre-scaled sprites
  or a custom flush without changing the pet logic.

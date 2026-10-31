# tomogotchi

A tamagotchi-style toy for the Waveshare 1.8" touch AMOLED ESP32-C6 board.

## Agent skills

### Issue tracker

Issues are tracked in GitHub Issues on `im-a-shitty-dev/tomogotchi`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: one `CONTEXT.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.

## Project notes

- Firmware is ESP-IDF (v5.5+) + LVGL 9 in C, using Waveshare's board package. See `docs/adr/0001-esp-idf-and-lvgl.md`.
- The ESP32-C6 has no PSRAM: never allocate a full 368x448 frame. Art is drawn on a 52x64 canvas scaled 7x.
- Keep `firmware/components/pet` free of ESP-IDF/hardware includes so `tests/host` can build it. Run the host tests after changing it.
- Art lives in `assets/sprites/` as PNGs. After changing them run `python tools/sprites/build.py` and commit the generated C too; CI checks it. See `assets/sprites/README.md` and `docs/adr/0002-sprite-pipeline.md`.
- Keep `firmware/components/sprites` and `firmware/components/scene` hardware-free too; their tests are in `tests/host`. Animation timing lives in `scene`, not `pet`.

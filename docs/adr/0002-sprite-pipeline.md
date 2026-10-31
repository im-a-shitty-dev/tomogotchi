# 2. Layered PNG sprites, converted to indexed C at build time

Date: 2026-10-31
Status: accepted

## Context

Tomo has many moods, outfits, faces, props and scenes, and the list keeps
growing. The art needs to be editable in normal pixel-art tools, and drawing
needs to be cheap on an ESP32-C6 with no PSRAM (see ADR 0001).

## Decision

- Art lives in `assets/sprites/` as PNGs on the 52x64 canvas, split into
  layers: backgrounds, background fx, bodies (outfit or pose), face and prop
  overlays, and front layers. A frame is a stack of these, so outfits x faces
  x props don't multiply into separate images.
- `tools/sprites/build.py` (Python + Pillow) converts them into
  `sprites_gen.c/.h`: one shared palette as RGB565 with index 0 transparent,
  each frame trimmed to its bounding box, one byte per pixel, identical frames
  stored once. A manifest, `sprites.toml`, names every sprite, fixes enum
  order, and maps bodies that need their own faces (masks) to their art.
- `sprite_draw()` copies straight into the LVGL canvas buffer, clipped once
  per sprite, with no per-pixel LVGL calls.
- The generated C is committed so the firmware build doesn't need Python.
  CI fails if it doesn't match the PNGs.
- The first PNGs were cut from the character mockup by
  `tools/sprites/seed/seed.py`. After that, the PNGs are the source of truth.

## Consequences

- About 115 KB of flash for the first set. Flash is 16 MB, so size isn't a
  worry; RAM use is unchanged.
- 255 colours shared across all art. If that gets tight, palettes can go per
  group without changing the drawing code much.
- Free-moving effects (bats, Zs, steam) are sprite strips too, placed by the
  scene code. Only screen-wide effects (the Tube lights dimming) are code.

# Sprites

All of Tomo's pixel art. Each PNG is drawn on the 52x64 art canvas, which the
firmware scales 7x to the screen. The PNGs are the source of truth:
`tools/sprites/build.py` turns them into C (`firmware/components/sprites/`).

## How a frame is drawn

Back to front:

1. `bg/` a full-screen background
2. `fx/` the animated parts of that background (waves, tunnel lights), if any
3. `body/` Tomo in an outfit or pose, outline and hair included
4. `face/` a face overlay, then `prop/` props, both at the body's head offset
5. `front/` anything in front of him (the tub, the blanket)

Faces and props are overlays: only the pixels that change. Most bodies share
the same faces. Bodies whose face looks different (masks, hoods over the
cheeks) get their own under `face/<body>/`, listed in `sprites.toml`.

## Editing

- Use any editor that saves PNG (Aseprite, Piskel, Photoshop...).
- Keep each frame 52x64. An animation is a horizontal strip: N frames make a
  PNG N*52 wide.
- Pixels must be fully opaque or fully transparent. No anti-aliasing.
- All PNGs share one palette of up to 255 colours. The build fails if they go
  over; reuse colours where you can.
- Keep shapes in the same place on the canvas as the art they replace, since
  position on the canvas is position on screen.

## Adding a sprite

1. Save the PNG in the right folder.
2. Add its name to the list in `sprites.toml`. Order sets the C enum values.
3. Run `python tools/sprites/build.py` and commit the PNG, `sprites.toml` and
   the generated C together. CI fails if they don't match.

`python tools/sprites/preview.py` writes a contact sheet of every scene to
`build/sprites-preview.png`, handy for checking edits.

## Where the first art came from

The first set was cut from the character mockup with
`tools/sprites/seed/seed.py`. Re-running it overwrites every PNG here, so once
the PNGs have been edited by hand, seed into a scratch folder
(`--out /tmp/seed`) and copy over only what you need.

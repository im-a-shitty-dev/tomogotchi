"""Animated preview of the firmware's own scene code, as a GIF.

    cmake -S tests/host -B build/host && cmake --build build/host
    python tools/sprites/animate.py [out.gif]   # default build/scenes.gif

Runs build/host/scene_dump (the real scene_build + scene_render, compiled for
your computer) and lays the scenes out in a grid at about 9 fps.
"""

import subprocess
import sys
from pathlib import Path

from PIL import Image

W, H = 52, 64
SCENES, TICKS = 12, 66
COLS, SCALE, PAD = 6, 3, 4
REPO = Path(__file__).resolve().parents[2]
TICK_MS = 110


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "build" / "scenes.gif"
    raw = subprocess.run([str(REPO / "build/host/scene_dump"), str(TICKS)], check=True, capture_output=True).stdout
    size = W * H * 2
    if len(raw) != SCENES * TICKS * size:
        sys.exit(f"expected {SCENES} scenes x {TICKS} frames from scene_dump, got {len(raw) // size} frames")

    def frame(scene, tick):
        data = raw[(scene * TICKS + tick) * size:][:size]
        img = Image.new("RGB", (W, H))
        px = [int.from_bytes(data[i:i + 2], "little") for i in range(0, size, 2)]
        img.putdata([(((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31) for c in px])
        return img.resize((W * SCALE, H * SCALE), Image.NEAREST)

    rows = (SCENES + COLS - 1) // COLS
    sheet = (COLS * (W * SCALE + PAD) + PAD, rows * (H * SCALE + PAD) + PAD)
    frames = []
    for t in range(TICKS):
        img = Image.new("RGB", sheet, (40, 40, 48))
        for s in range(SCENES):
            img.paste(frame(s, t), (PAD + (s % COLS) * (W * SCALE + PAD), PAD + (s // COLS) * (H * SCALE + PAD)))
        frames.append(img)
    out.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=TICK_MS, loop=0)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()

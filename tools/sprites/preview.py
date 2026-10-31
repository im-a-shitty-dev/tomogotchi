"""Composite the sprite PNGs into a contact sheet, to eyeball changes.

    python tools/sprites/preview.py [out.png]   # default build/sprites-preview.png

Draws each scene the way the firmware does (background, fx, body, face and
props at the head offset, front layer), straight from assets/sprites.
"""

import sys
import tomllib
from pathlib import Path

from PIL import Image

W, H = 52, 64
REPO = Path(__file__).resolve().parents[2]
ASSETS = REPO / "assets" / "sprites"
SCALE, PAD, COLS = 4, 6, 8

# (label, bg, fx, body, face, props, front, behind, (dx, dy))
SCENES = [
    ("deadpan", "deadpan", None, "casual", "blank", ["phone"], None, None, (0, 0)),
    ("smirk", "deadpan", None, "casual", "smirk", ["gummy"], None, None, (0, 0)),
    ("morning", "morning", None, "casual", "grumpy", ["bedhead", "cup"], None, None, (0, 0)),
    ("commute", "commute", "tunnel", "coat", "blank", ["phone"], None, None, (0, 1)),
    ("bath", "bath", None, "head", "happy", [], "tub", None, (0, 7)),
    ("bath back", "bath", None, "bath_back", None, [], None, "tub", (0, 0)),
    ("couch", "couch", None, "couch", "read", [], None, None, (0, 0)),
    ("beach", "beach", "waves", "beach", "shades", [], None, None, (0, 0)),
    ("night", "night", None, "pjs_phone", "screen", [], None, None, (0, 0)),
    ("switch", "night", None, "pjs_switch", "screen", [], None, None, (0, 0)),
    ("asleep", "asleep", None, "head", "sleep", [], "blanket", None, (0, 6)),
    ("back", "deadpan", None, "casual_back", None, [], None, None, (0, 0)),
]


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "build" / "sprites-preview.png"
    m = tomllib.loads((ASSETS / "sprites.toml").read_text())
    scenes = list(SCENES)
    for c in ["dracula", "chiikawa", "sadako", "trex", "barrel", "pikachu", "jason", "trike", "sylveon"]:
        face = "smirk" if c in ("chiikawa", "pikachu", "sylveon", "trex", "trike") else "fang"
        scenes.append((c, "spooky", "stars", c, face, [], None, None, (0, 0)))
        scenes.append((f"{c} back", "spooky", "stars", f"{c}_back", None, [], None, None, (0, 0)))

    cache = {}

    def png(rel):
        if rel not in cache:
            cache[rel] = Image.open(ASSETS / f"{rel}.png").convert("RGBA")
        return cache[rel]

    def layer(img, rel, dx=0, dy=0, frame=0):
        src = png(rel)
        frame %= src.width // W
        img.alpha_composite(src.crop((frame * W, 0, frame * W + W, H)), (0, 0)) if (dx, dy) == (0, 0) else \
            img.alpha_composite(src.crop((frame * W, 0, frame * W + W, H)).transform(
                (W, H), Image.AFFINE, (1, 0, -dx, 0, 1, -dy)))

    def art(kind, body, name):
        o = m.get(f"{kind}_override", {}).get(body, {})
        path = o.get(name, name)
        return f"{kind}/{path}" if path else None

    rows = (len(scenes) + COLS - 1) // COLS
    sheet = Image.new("RGB", (COLS * (W * SCALE + PAD) + PAD, rows * (H * SCALE + PAD) + PAD), (40, 40, 48))
    for n, (label, bg, fx, body, face, props, front, behind, (dx, dy)) in enumerate(scenes):
        img = Image.new("RGBA", (W, H), (0, 0, 0, 255))
        layer(img, f"bg/{bg}")
        if fx:
            layer(img, f"fx/{fx}")
        if behind:
            layer(img, f"front/{behind}")
        layer(img, f"body/{body}", dx, dy)
        hx, hy = m.get("head_offset", {}).get(body, (0, 0))
        for kind, name in [("face", face)] + [("prop", p) for p in props]:
            rel = name and art(kind, body, name)
            if rel:
                layer(img, rel, dx + hx, dy + hy)
        if front:
            layer(img, f"front/{front}")
        x, y = PAD + (n % COLS) * (W * SCALE + PAD), PAD + (n // COLS) * (H * SCALE + PAD)
        sheet.paste(img.convert("RGB").resize((W * SCALE, H * SCALE), Image.NEAREST), (x, y))
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()

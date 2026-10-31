"""Turn the sprite PNGs into C for the firmware.

    python tools/sprites/build.py           # regenerate the C files
    python tools/sprites/build.py --check   # fail if they're out of date (CI)

Reads assets/sprites/sprites.toml and the PNGs it names, and writes
firmware/components/sprites/{include/sprites_gen.h,sprites_gen.c}:

- One shared palette of up to 255 colours, stored as RGB565. Index 0 means
  transparent.
- Each frame trimmed to its bounding box, one palette index per pixel.
- Frames with identical pixels are stored once.

PNG rules: RGBA or RGB, 64 px tall, a multiple of 52 px wide (52 px per frame),
and every pixel fully opaque or fully transparent.
"""

import argparse
import sys
import tomllib
from dataclasses import dataclass
from pathlib import Path

from PIL import Image

W, H = 52, 64
REPO = Path(__file__).resolve().parents[2]
ASSETS = REPO / "assets" / "sprites"
OUT_DIR = REPO / "firmware" / "components" / "sprites"
OUT_H = OUT_DIR / "include" / "sprites_gen.h"
OUT_C = OUT_DIR / "sprites_gen.c"
GROUPS = ["bg", "body", "face", "prop", "front", "fx", "hud"]


class SpriteError(Exception):
    pass


@dataclass(frozen=True)
class Frame:
    x: int
    y: int
    w: int
    h: int
    px: tuple  # RGB565 per pixel, None = transparent


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def load_png(rel: str) -> list[Frame]:
    path = ASSETS / f"{rel}.png"
    if not path.is_file():
        raise SpriteError(f"missing {path.relative_to(REPO)}")
    img = Image.open(path).convert("RGBA")
    if img.height != H or img.width % W or img.width == 0:
        raise SpriteError(f"{rel}.png is {img.width}x{img.height}; want {H} tall and a multiple of {W} wide")
    frames = []
    for n in range(img.width // W):
        px = []
        for y in range(H):
            for x in range(W):
                r, g, b, a = img.getpixel((n * W + x, y))
                if a not in (0, 255):
                    raise SpriteError(f"{rel}.png pixel ({n * W + x}, {y}) is semi-transparent (alpha {a})")
                px.append(rgb565(r, g, b) if a else None)
        frames.append(trim(px))
    return frames


def trim(px) -> Frame:
    xs = [i % W for i, c in enumerate(px) if c is not None]
    ys = [i // W for i, c in enumerate(px) if c is not None]
    if not xs:
        return Frame(0, 0, 0, 0, ())
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    return Frame(x0, y0, x1 - x0 + 1, y1 - y0 + 1,
                 tuple(px[y * W + x] for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)))


def c_ident(name: str) -> str:
    out = "".join(ch if ch.isalnum() else "_" for ch in name)
    if not out or out[0].isdigit():
        raise SpriteError(f"bad sprite name {name!r}")
    return out


def load(manifest_path: Path):
    m = tomllib.loads(manifest_path.read_text())
    unknown = set(m) - set(GROUPS) - {"head_offset", "face_override", "prop_override"}
    if unknown:
        raise SpriteError(f"unknown keys in sprites.toml: {', '.join(sorted(unknown))}")
    groups = {g: list(m.get(g, [])) for g in GROUPS}
    for g, names in groups.items():
        if len(set(names)) != len(names):
            raise SpriteError(f"duplicate names in {g}")

    sprites = {}  # "group/name" -> [Frame]
    for g, names in groups.items():
        for n in names:
            c_ident(n)
            sprites[f"{g}/{n}"] = load_png(f"{g}/{n}")

    overrides = {}  # kind -> {(body, name): "kind/path" or None}
    for kind in ("face", "prop"):
        table = {}
        for body, entries in m.get(f"{kind}_override", {}).items():
            if body not in groups["body"]:
                raise SpriteError(f"{kind}_override.{body}: no such body")
            for name, path in entries.items():
                if name not in groups[kind]:
                    raise SpriteError(f"{kind}_override.{body}.{name}: no such {kind}")
                if path == "":
                    table[(body, name)] = None
                else:
                    key = f"{kind}/{path}"
                    if key not in sprites:
                        sprites[key] = load_png(key)
                    table[(body, name)] = key
        overrides[kind] = table

    heads = {}
    for body, xy in m.get("head_offset", {}).items():
        if body not in groups["body"]:
            raise SpriteError(f"head_offset.{body}: no such body")
        if len(xy) != 2 or not all(isinstance(v, int) and -W < v < W for v in xy):
            raise SpriteError(f"head_offset.{body}: want [x, y]")
        heads[body] = tuple(xy)

    used = {f"{k}.png" for k in sprites}
    strays = sorted(str(p.relative_to(ASSETS)) for p in ASSETS.rglob("*.png") if str(p.relative_to(ASSETS)) not in used)
    if strays:
        raise SpriteError("PNGs not listed in sprites.toml: " + ", ".join(strays))
    return groups, sprites, overrides, heads


def generate(groups, sprites, overrides, heads) -> tuple[str, str]:
    colours = sorted({c for frames in sprites.values() for f in frames for c in f.px if c is not None})
    if len(colours) > 255:
        raise SpriteError(f"{len(colours)} colours in total; the palette holds 255")
    index = {c: i + 1 for i, c in enumerate(colours)}

    # Pixel data, shared between identical frames.
    pixel_arrays = {}  # Frame -> C name
    data = []
    for frames in sprites.values():
        for f in frames:
            if f.w and f not in pixel_arrays:
                name = f"px{len(pixel_arrays)}"
                pixel_arrays[f] = name
                data.append((name, f))

    hdr = [
        "// Generated by tools/sprites/build.py from assets/sprites. Do not edit.",
        "#pragma once",
        "",
        '#include "sprite.h"',
        "",
        "#ifdef __cplusplus",
        'extern "C" {',
        "#endif",
        "",
    ]
    for g in GROUPS:
        upper = g.upper()
        hdr.append("typedef enum {")
        hdr += [f"    SPRITE_{upper}_{c_ident(n).upper()}," for n in groups[g]]
        hdr += [f"    SPRITE_{upper}_COUNT", f"}} sprite_{g}_t;", ""]
    hdr += [
        f"#define SPRITE_PALETTE_SIZE {len(colours) + 1}",
        "extern const uint16_t sprite_palette[SPRITE_PALETTE_SIZE];",
        "",
    ]
    for g in GROUPS:
        if g in ("face", "prop"):
            continue
        hdr.append(f"extern const sprite_t sprite_{g}[SPRITE_{g.upper()}_COUNT > 0 ? SPRITE_{g.upper()}_COUNT : 1];")
    hdr += [
        "",
        "// Where a body's head is, relative to the standard standing pose. Draw its",
        "// face and props shifted by this much.",
        "extern const sprite_offset_t sprite_head_offset[SPRITE_BODY_COUNT];",
        "",
        "// Face and prop art for a body. Most bodies share one set; masks and hoods",
        "// have their own. Returns NULL when that body shows no such face or prop.",
        "const sprite_t *sprite_face(sprite_body_t body, sprite_face_t face);",
        "const sprite_t *sprite_prop(sprite_body_t body, sprite_prop_t prop);",
        "",
        "#ifdef __cplusplus",
        "}",
        "#endif",
        "",
    ]

    src = [
        "// Generated by tools/sprites/build.py from assets/sprites. Do not edit.",
        '#include "sprites_gen.h"',
        "",
        "#include <stddef.h>",
        "",
        "const uint16_t sprite_palette[SPRITE_PALETTE_SIZE] = {",
        "    0x0000,  // 0: transparent",
    ]
    for i in range(0, len(colours), 8):
        src.append("    " + " ".join(f"0x{c:04x}," for c in colours[i:i + 8]))
    src += ["};", ""]

    total = 0
    for name, f in data:
        src.append(f"static const uint8_t {name}[{f.w * f.h}] = {{  // {f.w}x{f.h}")
        idx = [index[c] if c is not None else 0 for c in f.px]
        total += len(idx)
        for r in range(f.h):
            row = idx[r * f.w:(r + 1) * f.w]
            src.append("    " + "".join(f"{v}," for v in row))
        src += ["};", ""]

    def frame_list(key):
        cname = "fr_" + c_ident(key.replace("/", "__"))
        items = [f"{{{f.x}, {f.y}, {f.w}, {f.h}, {pixel_arrays[f] if f.w else 'NULL'}}}" for f in sprites[key]]
        src.append(f"static const sprite_frame_t {cname}[] = {{{', '.join(items)}}};")
        return f"{{{cname}, {len(sprites[key])}}}"

    sprite_defs = {key: frame_list(key) for key in sprites}
    src.append("")

    for g in GROUPS:
        if g in ("face", "prop"):
            continue
        src.append(f"const sprite_t sprite_{g}[SPRITE_{g.upper()}_COUNT > 0 ? SPRITE_{g.upper()}_COUNT : 1] = {{")
        src += [f"    {sprite_defs[f'{g}/{n}']},  // {n}" for n in groups[g]]
        src += ["};", ""]

    src.append("const sprite_offset_t sprite_head_offset[SPRITE_BODY_COUNT] = {")
    src += [f"    {{{heads.get(b, (0, 0))[0]}, {heads.get(b, (0, 0))[1]}}},  // {b}" for b in groups["body"]]
    src += ["};", ""]

    for kind in ("face", "prop"):
        src.append(f"static const sprite_t {kind}_sprites[] = {{")
        keys = [k for k in sprites if k.startswith(f"{kind}/")]
        slot = {k: i for i, k in enumerate(keys)}
        src += [f"    {sprite_defs[k]},  // {k}" for k in keys]
        if not keys:
            src.append("    {NULL, 0},")
        src += ["};", ""]
        # Table of indices into {kind}_sprites, 0xff = none.
        n_kind = len(groups[kind])
        src.append(f"static const uint8_t {kind}_table[SPRITE_BODY_COUNT][{max(n_kind, 1)}] = {{")
        if len(keys) >= 0xff:
            raise SpriteError(f"too many {kind} sprites")
        for body in groups["body"]:
            row = []
            for n in groups[kind]:
                key = overrides[kind].get((body, n), f"{kind}/{n}")
                row.append("0xff" if key is None else str(slot[key]))
            src.append(f"    {{{', '.join(row) or '0xff'}}},  // {body}")
        src += ["};", ""]
        src += [
            f"const sprite_t *sprite_{kind}(sprite_body_t body, sprite_{kind}_t {kind})",
            "{",
            f"    if ((unsigned)body >= SPRITE_BODY_COUNT || (unsigned){kind} >= SPRITE_{kind.upper()}_COUNT) {{",
            "        return NULL;",
            "    }",
            f"    uint8_t i = {kind}_table[body][{kind}];",
            f"    return i == 0xff ? NULL : &{kind}_sprites[i];",
            "}",
            "",
        ]

    src[1:1] = [f"// {len(colours)} colours, {len(data)} unique frames, {total} bytes of pixels."]
    return "\n".join(hdr), "\n".join(src)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--check", action="store_true", help="fail if the generated files are out of date")
    args = ap.parse_args()
    try:
        h, c = generate(*load(ASSETS / "sprites.toml"))
    except SpriteError as e:
        sys.exit(f"sprites: {e}")

    outputs = {OUT_H: h, OUT_C: c}
    if args.check:
        stale = [p for p, text in outputs.items() if not p.is_file() or p.read_text() != text]
        if stale:
            names = ", ".join(str(p.relative_to(REPO)) for p in stale)
            sys.exit(f"sprites: {names} out of date; run python tools/sprites/build.py")
        print("sprites: generated C is up to date")
        return
    for p, text in outputs.items():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    print(f"sprites: wrote {OUT_H.relative_to(REPO)} and {OUT_C.relative_to(REPO)}")


if __name__ == "__main__":
    main()

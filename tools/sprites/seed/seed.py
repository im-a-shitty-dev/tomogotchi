"""Cut sprite PNGs out of the character mockup.

The mockup (design/tomo-moods.html in the project files) draws Tomo with code.
This runs that code (via render_mockup.mjs, needs Node) and splits the renders
into the layers the firmware composites, then writes assets/sprites/:

    bg/<name>.png          full-screen backgrounds
    fx/<name>.png          animated bits (waves, Zs, bats...), as frame strips
    body/<body>.png        Tomo in an outfit or pose, outline and hair baked in
    face/<face>.png        face overlay, shared by most bodies
    face/<body>/<face>.png face overlay where a body needs its own (masks)
    prop/<prop>.png        held props and snacks
    front/<name>.png       layers drawn over Tomo (tub, blanket)
    sprites.toml           the manifest build.py reads

Overlays are "what changes": a face overlay holds exactly the pixels that
differ between the body with that face and the body with no face drawn, so
body + face + prop composites match the mockup pixel for pixel. The script
checks that before writing anything.

Re-running it overwrites every PNG it makes, so once someone edits the PNGs
by hand, add new art by hand too (or seed into a scratch folder with --out and
copy over what you need). It doesn't delete PNGs it no longer makes; build.py
flags those as unlisted.

    python tools/sprites/seed/seed.py path/to/tomo-moods.html [--out DIR]
"""

import argparse
import json
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

import foam

W, H = 52, 64
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]

COSTUMES = ["dracula", "chiikawa", "sadako", "trex", "barrel", "pikachu", "jason", "trike", "sylveon"]
FACES = ["blank", "blink", "smirk", "fang", "happy", "annoyed", "grumpy", "grumpyblink",
         "dizzy", "sleep", "screen", "read", "shades"]
ITEMS = ["gummy", "crisps", "coffee"]
BAT = "#05030a"

# name: (mood, scene). Scene only matters for me-time.
BACKGROUNDS = {
    "deadpan": ("deadpan", None),
    "morning": ("morning", None),
    "commute": ("commute", None),
    "bath": ("metime", "bath"),
    "couch": ("metime", "couch"),
    "beach": ("metime", "beach"),
    "night": ("night", None),
    "asleep": ("asleep", None),
    "spooky": ("spooky", None),
}
# Animated parts of backgrounds: (background, mockup ticks to sample).
# Each frame holds only the pixels that ever change, drawn over the background.
FX = {
    "waves": ("beach", range(0, 36, 3)),
    "tunnel": ("commute", range(40)),  # lights streak past, window tops flicker
    "stars": ("spooky", range(20)),    # twinkle
}
# Free-moving bits drawn in code in the mockup, cut as strips: (kind, ticks).
# The firmware decides where in the stack they go.
LOOSE_FX = {
    "steam": ("steam", range(20)),   # over the tub
    "zzz": ("zzz", range(30)),       # over the blanket
    "bat": ("bat", range(66)),       # one bat crossing the sky; drawn twice
    "splash": ("splash", range(2)),  # shaken in the bath
}
# New art drawn by code here rather than cut from the mockup.
DRAWN_FX = {"foam": foam.frames}  # bubbles overflowing the tub

# Costumes whose tail wags (and Sylveon's ribbons), cut as their own strips
# and left out of the bodies. From the front the tail goes behind him and
# from the back in front; Sylveon's ribbons are always in front.
TAILED = ["pikachu", "sylveon", "trex", "trike"]
TAIL_FRAMES = 4


def no_tail(c):
    return {"noTail": True} if c in TAILED else {}


def tail_fx():
    names = []
    for c in TAILED:
        for suffix in ("", "_back"):
            names.append(f"tail_{c}{suffix}")
    return names + ["ribbons_sylveon", "ribbons_sylveon_back"]


# Front-facing bodies, as drawTomo options.
FRONT = {
    "casual": {},
    "coat": {"outfit": "coat", "cans": True},  # commute: trench coat, headphones on
    **{c: {"outfit": c, **no_tail(c)} for c in COSTUMES},
    "pjs": {"outfit": "pjs"},
    # At night his face is lit by the screen and both hands hold the device.
    # The hands move, so each device is its own body rather than a prop.
    "pjs_phone": {"outfit": "pjs", "glow": True, "device": "phone"},
    "pjs_switch": {"outfit": "pjs", "glow": True, "device": "switch"},
    # Head and shoulders only: in the bath, and tucked up in bed.
    "head": {"noBody": True},
}
# Reclining me-time poses, holding manga / a piña colada.
LYING = ["couch", "beach"]
BACK = {
    "casual_back": {"back": True},
    **{f"{c}_back": {"outfit": c, "back": True, **no_tail(c)} for c in COSTUMES},
    "pjs_back": {"outfit": "pjs", "glow": True, "back": True},
    # Standing up in the bath, facing away. The tub goes in front.
    "bath_back": {"outfit": "bath", "back": True, "bathBack": True},
}
# Props in his hand (phone, morning coffee) and the morning bed hair, per body.
HELD = {"phone": {"phone": True}, "cup": {"cup": True}, "bedhead": {"bedhead": True}}
HELD_ON = {"casual": ["phone", "cup", "bedhead"], "coat": ["phone"]}
PROPS = [*HELD, *ITEMS, *(f"{i}_icon" for i in ITEMS)]  # icons: no fingers, for the snack picker


def render_all(mockup: Path, reqs: list) -> dict:
    with tempfile.TemporaryDirectory() as d:
        path = Path(d) / "requests.json"
        path.write_text(json.dumps(reqs))
        out = subprocess.run(
            ["node", str(HERE / "render_mockup.mjs"), str(mockup), str(path)],
            check=True, capture_output=True, text=True,
        ).stdout
    return json.loads(out)


def tomo(name, opts):
    return {"name": name, "kind": "tomo", "opts": opts}


def overlay(full, base):
    """Pixels of `full` that differ from `base`. Fails if `full` would need a hole."""
    out = []
    for f, b in zip(full, base):
        if f is None and b is not None:
            raise SystemExit("an overlay needs a transparent pixel over the body; make it its own body")
        out.append(f if f != b else None)
    return out


def over(*layers):
    out = list(layers[0])
    for l in layers[1:]:
        out = [p if p is not None else o for o, p in zip(out, l)]
    return out


def shift(layer, dx, dy):
    out = [None] * (W * H)
    for i, c in enumerate(layer):
        if c is not None:
            x, y = i % W + dx, i // W + dy
            if not (0 <= x < W and 0 <= y < H):
                raise SystemExit("shifted layer falls off the canvas")
            out[y * W + x] = c
    return out


class Writer:
    def __init__(self, out: Path):
        self.out = out
        self.count = 0

    def save(self, frames, rel):
        if not isinstance(frames[0], list):
            frames = [frames]
        img = Image.new("RGBA", (W * len(frames), H), (0, 0, 0, 0))
        for n, layer in enumerate(frames):
            for i, c in enumerate(layer):
                if c is not None:
                    img.putpixel((n * W + i % W, i // W), (int(c[1:3], 16), int(c[3:5], 16), int(c[5:7], 16), 255))
        path = self.out / f"{rel}.png"
        path.parent.mkdir(parents=True, exist_ok=True)
        img.save(path, optimize=True)
        self.count += 1


def write_manifest(out: Path, bodies, head_offsets, hand_offsets, overrides):
    def names(xs):
        return "[" + ", ".join(f'"{x}"' for x in xs) + "]"

    lines = [
        "# Every sprite the firmware knows about. tools/sprites/build.py turns this",
        "# and the PNGs into firmware/components/sprites/sprites_gen.{c,h}.",
        "#",
        "# Each name is a PNG at <group>/<name>.png, drawn on the 52x64 canvas.",
        "# A PNG N*52 pixels wide is an N-frame animation strip.",
        "# List order sets the C enum values.",
        "",
        "bg = " + names(BACKGROUNDS),
        "fx = " + names([*FX, *LOOSE_FX, *DRAWN_FX, *tail_fx()]),
        "body = " + names(bodies),
        "face = " + names(FACES),
        "prop = " + names(PROPS),
        'front = ["tub", "blanket", "bath_arm"]',
        "",
        "# Faces and props are drawn at the body's head offset. Bodies not listed",
        "# use [0, 0]. Lying poses have the head further left and lower.",
        "[head_offset]",
    ]
    lines += [f"{b} = [{x}, {y}]" for b, (x, y) in head_offsets.items()]
    lines += [
        "",
        "# Snacks are cut in the standing hand. Bodies whose hand is elsewhere",
        "# (back views, lying with a free hand, the bath arm for head) list how far.",
        "[hand_offset]",
    ]
    lines += [f"{b} = [{x}, {y}]" for b, (x, y) in hand_offsets.items()]
    lines += [
        "",
        "# Bodies that need their own face or prop art (masks, hoods over the",
        "# cheeks). The value is a PNG under face/ or prop/ without .png, or \"\" to",
        "# draw nothing. Anything not listed uses face/<face>.png or prop/<prop>.png.",
    ]
    for kind in ("face", "prop"):
        lines += ["", f"[{kind}_override]"]
        for body, entries in overrides[kind].items():
            lines.append(f"{body} = {{ " + ", ".join(f'{k} = "{v}"' for k, v in entries.items()) + " }")
    (out / "sprites.toml").write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser(description="Cut sprite PNGs out of the character mockup.")
    ap.add_argument("mockup", type=Path)
    ap.add_argument("--out", type=Path, default=REPO / "assets" / "sprites")
    args = ap.parse_args()

    reqs = []
    for name, (mood, scene) in BACKGROUNDS.items():
        reqs.append({"name": f"bg:{name}", "kind": "scene", "mood": mood, "scene": scene, "skip": [BAT]})
    for name, (bg, ticks) in FX.items():
        mood, scene = BACKGROUNDS[bg]
        reqs += [{"name": f"fx:{name}:{t}", "kind": "scene", "mood": mood, "scene": scene, "t": t, "skip": [BAT]}
                 for t in ticks]
    for name, (kind, ticks) in LOOSE_FX.items():
        reqs += [{"name": f"fx:{name}:{t}", "kind": kind, "t": t} for t in ticks]
    reqs += [{"name": "front:tub", "kind": "tub"}, {"name": "front:blanket", "kind": "blanket"},
             {"name": "front:bath_arm", "kind": "batharm"}]
    reqs += [{"name": f"item:{i}", "kind": "item", "item": i} for i in ITEMS]
    reqs += [{"name": f"icon:{i}", "kind": "item", "item": i, "noFingers": True} for i in ITEMS]
    for body, opts in FRONT.items():
        reqs.append(tomo(f"{body}:", opts))
        reqs += [tomo(f"{body}:{f}", {**opts, "face": f}) for f in FACES]
        for prop in HELD_ON.get(body, []):
            reqs.append(tomo(f"{body}+{prop}", {**opts, **HELD[prop]}))
            reqs.append(tomo(f"{body}:smirk+{prop}", {**opts, **HELD[prop], "face": "smirk"}))
    for pose in LYING:
        reqs.append({"name": f"{pose}_hand:", "kind": "lying", "pose": pose, "hold": "hand"})
        reqs.append({"name": f"{pose}:", "kind": "lying", "pose": pose})
        reqs += [{"name": f"{pose}:{f}", "kind": "lying", "pose": pose, "face": f} for f in FACES]
    reqs += [tomo(f"{b}:", o) for b, o in BACK.items()]
    for c in TAILED:
        for back in (False, True):
            reqs += [tomo(f"tail:{c}:{int(back)}:{k}", {"outfit": c, "back": back, "tailOnly": True, "tailFrame": k})
                     for k in range(TAIL_FRAMES)]
            if c == "sylveon":
                reqs += [tomo(f"ribbons:{c}:{int(back)}:{k}",
                              {"outfit": c, "back": back, "ribbonsOnly": True, "ribbonFrame": k})
                         for k in range(TAIL_FRAMES)]
    R = render_all(args.mockup, reqs)

    # Work everything out before touching the output folder.
    files = {}  # rel path -> layer or list of frames

    for name in BACKGROUNDS:
        files[f"bg/{name}"] = R[f"bg:{name}"]
    for name, (bg, ticks) in FX.items():
        frames = [R[f"fx:{name}:{t}"] for t in ticks]
        moving = [any(f[i] != frames[0][i] for f in frames) for i in range(W * H)]
        files[f"bg/{bg}"] = frames[0]
        files[f"fx/{name}"] = [[c if moving[i] else None for i, c in enumerate(f)] for f in frames]
    for name, (_, ticks) in LOOSE_FX.items():
        files[f"fx/{name}"] = [R[f"fx:{name}:{t}"] for t in ticks]
    for name, draw in DRAWN_FX.items():
        files[f"fx/{name}"] = draw()
    for f in ("tub", "blanket", "bath_arm"):
        files[f"front/{f}"] = R[f"front:{f}"]
    # Snacks can be handed to any body, so each is drawn on its own with its
    # own outline, in his fingers at the standing hand (35, 46).
    for i in ITEMS:
        files[f"prop/{i}"] = R[f"item:{i}"]
        files[f"prop/{i}_icon"] = R[f"icon:{i}"]

    head_offsets = {pose: tuple(R["_lying_head"][pose]) for pose in LYING}
    shared = {"face": {}, "prop": {}}
    overrides = {"face": {}, "prop": {}}

    def place(kind, body, name, layer):
        """Use the shared art if it matches, else record a per-body override."""
        if body == "casual":
            shared[kind][name] = layer
            files[f"{kind}/{name}"] = layer
            return
        if layer == shared[kind][name]:
            return
        mine = overrides[kind].setdefault(body, {})
        if all(c is None for c in layer):
            mine[name] = ""
            return
        for other, path in mine.items():  # reuse identical art, e.g. one mask for many faces
            if path and files[f"{kind}/{path}"] == layer:
                mine[name] = path
                return
        mine[name] = f"{body}/{name}"
        files[f"{kind}/{body}/{name}"] = layer

    for body in [*FRONT, *LYING]:
        base = R[f"{body}:"]  # no face drawn
        files[f"body/{body}"] = base
        dx, dy = head_offsets.get(body, (0, 0))
        for f in FACES:
            layer = overlay(R[f"{body}:{f}"], base)
            assert over(base, layer) == R[f"{body}:{f}"]
            place("face", body, f, shift(layer, -dx, -dy))
        for prop in HELD_ON.get(body, []):
            layer = overlay(R[f"{body}+{prop}"], base)
            smirk = overlay(R[f"{body}:smirk"], base)
            if over(base, smirk, layer) != R[f"{body}:smirk+{prop}"]:
                raise SystemExit(f"{body}: smirk + {prop} doesn't composite cleanly")
            place("prop", body, prop, layer)
    for body in BACK:
        files[f"body/{body}"] = R[f"{body}:"]
    # Lying with a hand free for a snack: same head and faces as the pose.
    for pose in LYING:
        files[f"body/{pose}_hand"] = R[f"{pose}_hand:"]
        head_offsets[f"{pose}_hand"] = head_offsets[pose]
        for kind in overrides:
            if pose in overrides[kind]:
                overrides[kind][f"{pose}_hand"] = overrides[kind][pose]

    hands = R["_hands"]
    sx, sy = hands["stand"]

    def from_stand(key):
        return (hands[key][0] - sx, hands[key][1] - sy)

    hand_offsets = {"head": from_stand("bath")}  # head: sitting in the bath, arm out of the foam
    hand_offsets.update({pose + "_hand": from_stand(pose) for pose in LYING})
    # From the back the snack peeks out from behind him; Dracula's cape would
    # hide it, so he holds it out past the cape (scene.c draws it in front).
    hand_offsets.update({b: from_stand(b if b in ("bath_back", "dracula_back") else "back") for b in BACK})

    for c in TAILED:
        for back, suffix in ((False, ""), (True, "_back")):
            files[f"fx/tail_{c}{suffix}"] = [R[f"tail:{c}:{int(back)}:{k}"] for k in range(TAIL_FRAMES)]
            if c == "sylveon":
                files[f"fx/ribbons_{c}{suffix}"] = [R[f"ribbons:{c}:{int(back)}:{k}"] for k in range(TAIL_FRAMES)]

    out = args.out
    w = Writer(out)
    for rel, layer in files.items():
        w.save(layer, rel)
    write_manifest(out, [*FRONT, *LYING, *BACK, *(f"{p}_hand" for p in LYING)], head_offsets, hand_offsets, overrides)
    print(f"wrote {w.count} PNGs and sprites.toml to {out}")


if __name__ == "__main__":
    main()

"""Bath foam spilling over the tub: new art that isn't in the mockup.

seed.py calls frames() to write fx/foam.png, a 24-frame strip drawn over the
tub: foam mounds heaped on the rim and spilling down both ends, drips sliding
down the front, and little bubbles floating up and popping.
"""

W, H = 52, 64
OL = "#07050b"      # outline, as in the mockup
FOAM = "#eef8ff"    # the mockup's bubble colour
SHADE = "#c9dde8"
SHINE = "#ffffff"
SOFT = "#a9c0cc"    # outline where foam sits on the white tub
FRAMES = 24

# Mounds on the rim: (cx, cy, rx, ry). Low in the middle so his face shows,
# heaped up at the ends.
MOUNDS = [(5, 39, 4, 3.5), (10, 37.5, 4, 4), (16, 39.5, 3.5, 2.5), (36, 39.5, 3.5, 2.5),
          (42, 37.5, 4, 4), (47, 39, 4, 3.5)]
# Foam hanging over each end of the tub: (x, top, bottom).
SPILLS = [(2, 40, 45), (3, 40, 47), (4, 41, 44), (49, 40, 46), (48, 40, 48), (47, 41, 44)]
# Drips running down the tub front: (x, start y, phase).
DRIPS = [(8, 43, 0), (27, 43, 3), (44, 43, 5)]
# Bubbles floating up: (x, phase).
RISERS = [(7, 0), (45, 8), (13, 15), (39, 4)]


def _layer():
    return [None] * (W * H)


def _px(layer, x, y, c):
    if 0 <= x < W and 0 <= y < H:
        layer[y * W + x] = c


def _ellipse(layer, cx, cy, rx, ry, c):
    for y in range(int(cy - ry) - 1, int(cy + ry) + 2):
        for x in range(int(cx - rx) - 1, int(cx + rx) + 2):
            dx, dy = (x + 0.5 - cx) / rx, (y + 0.5 - cy) / ry
            if dx * dx + dy * dy <= 1:
                _px(layer, x, y, c)


def _outline(layer):
    src = list(layer)
    for y in range(H):
        for x in range(W):
            if src[y * W + x] is not None:
                continue
            n = [(x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)]
            if any(0 <= a < W and 0 <= b < H and src[b * W + a] is not None for a, b in n):
                on_tub = 5 <= x <= 46 and 41 <= y <= 51
                layer[y * W + x] = SOFT if on_tub else OL


def frames():
    out = []
    for t in range(FRAMES):
        foam = _layer()
        for cx, cy, rx, ry in MOUNDS:
            _ellipse(foam, cx, cy, rx, ry, FOAM)
        for x, top, bottom in SPILLS:
            for y in range(top, bottom + 1):
                _px(foam, x, y, FOAM)
        # Drips: grow down the front, then let go and start again.
        for x, y0, ph in DRIPS:
            s = (t + ph) % 8
            for y in range(y0, y0 + min(s, 5) + 1):
                _px(foam, x, y, FOAM)
            if s >= 6:
                _px(foam, x, y0 + s, FOAM)
        # Mounds wobble: a lump swells on alternate halves of the loop.
        if t % 12 < 6:
            _ellipse(foam, 10, 35.5, 2, 1.5, FOAM)
        else:
            _ellipse(foam, 42, 35.5, 2, 1.5, FOAM)
        _outline(foam)
        # Shading and shine inside the outline.
        for cx, cy, rx, ry in MOUNDS:
            _px(foam, int(cx - rx / 2), int(cy - ry / 2), SHINE)
            _px(foam, int(cx + rx / 2), int(cy + ry / 2) - 1, SHADE)
        # Bubbles float up from the foam and pop at the top.
        for x, ph in RISERS:
            s = (t + ph) % 12
            if s < 9:
                bx = x + (1 if s % 4 >= 2 else 0)
                by = 34 - s
                _px(foam, bx, by, OL)
                _px(foam, bx + 1, by, OL)
                _px(foam, bx - 1, by + 1, OL)
                _px(foam, bx + 2, by + 1, OL)
                _px(foam, bx, by + 2, OL)
                _px(foam, bx + 1, by + 2, OL)
                _px(foam, bx, by + 1, SHINE)
                _px(foam, bx + 1, by + 1, FOAM)
            elif s == 9:  # pop
                for dx, dy in [(-1, 0), (2, 0), (0, -1), (1, 2)]:
                    _px(foam, x + dx, 25 + dy, FOAM)
        out.append(foam)
    return out

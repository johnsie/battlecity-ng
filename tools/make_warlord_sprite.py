#!/usr/bin/env python3
"""Builds data/imgWarlord.bmp, the sprite sheet of the Warlord (the bounty boss), from a picture of the tank seen from above, barrel up.

16 columns of 64x64: column n is the tank turned n * 22.5 degrees anticlockwise, like the columns of imgTanks.bmp (the game picks the
column from the tank's direction / 2). The background is the game's transparency colour (magenta).

Usage: tools/make_warlord_sprite.py <tank.png> [out.bmp]
"""
import sys
import numpy as np
from PIL import Image
from collections import deque

CELL = 64
RADIUS = 31.0          # the tank fits in a circle of this radius, so no turn clips it

src = Image.open(sys.argv[1]).convert("RGBA")
out_path = sys.argv[2] if len(sys.argv) > 2 else "data/imgWarlord.bmp"
a = np.array(src)[:, :, 3]
solid = a > 128
# the tank itself: everything joined to the middle of the picture (drops specks of noise around it)
H, W = solid.shape
keep = np.zeros_like(solid)
start = (H // 2, W // 2)
if not solid[start]:
    ys, xs = np.where(solid)
    i = int(np.argmin((ys - H // 2) ** 2 + (xs - W // 2) ** 2))
    start = (int(ys[i]), int(xs[i]))
todo = deque([start])
keep[start] = True
while todo:
    y, x = todo.popleft()
    for ny, nx in ((y + 1, x), (y - 1, x), (y, x + 1), (y, x - 1)):
        if 0 <= ny < H and 0 <= nx < W and solid[ny, nx] and not keep[ny, nx]:
            keep[ny, nx] = True
            todo.append((ny, nx))
arr = np.array(src)
arr[:, :, 3] = np.where(keep, 255, 0)
ys, xs = np.where(keep)
tank = Image.fromarray(arr).crop((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1))

# canvas big enough to turn the tank about its centre without clipping
w, h = tank.size
radius = (w * w + h * h) ** 0.5 / 2
side = int(radius * 2) + 4
canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
canvas.paste(tank, ((side - w) // 2, (side - h) // 2))

sheet = Image.new("RGB", (CELL * 16, CELL), (255, 0, 255))
scale = RADIUS * 2 / side
TILT = 0.84            # the tank is seen from above and in front, like the others: the top is squashed ...
DEPTH = 6              # ... and the side of the hull shows below it (pixels)
for col in range(16):
    turned = canvas.rotate(col * 22.5, resample=Image.BICUBIC)
    n = max(1, round(side * scale))
    top = turned.resize((n, max(1, round(n * TILT))), Image.LANCZOS)
    t = np.array(top).astype(np.float32)
    mask = t[:, :, 3] >= 140
    # light from the top left: a faint brightening of the upper-left half, a darkening of the lower-right
    yy, xx = np.mgrid[0:t.shape[0], 0:t.shape[1]]
    shade = 1.10 - 0.22 * ((xx / t.shape[1]) * 0.45 + (yy / t.shape[0]) * 0.55)
    lit = np.clip(t[:, :, :3] * shade[:, :, None], 0, 255)
    # the side wall: the hull shifted down a pixel at a time, darker and bluer with depth
    H = t.shape[0] + DEPTH
    layer = np.zeros((H, n, 3), np.float32)
    covered = np.zeros((H, n), bool)
    for d in range(DEPTH, 0, -1):
        wall = lit * (0.50 - 0.02 * (DEPTH - d)) + np.array([0, 4, 14], np.float32)
        sl = (slice(d, d + t.shape[0]), slice(0, n))
        layer[sl][mask] = wall[mask]
        covered[sl] |= mask
    sl = (slice(0, t.shape[0]), slice(0, n))
    layer[sl][mask] = lit[mask]
    covered[sl] |= mask
    rgb = np.where(covered[:, :, None], layer, np.array([255, 0, 255], np.float32)).astype(np.uint8)
    img = Image.fromarray(rgb)
    sheet.paste(img, (col * CELL + (CELL - n) // 2, (CELL - H) // 2))
sheet.save(out_path)
print("wrote", out_path, sheet.size)

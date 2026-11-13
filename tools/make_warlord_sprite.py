#!/usr/bin/env python3
"""Builds data/imgWarlord.bmp, the sprite sheet of the Warlord (the bounty boss), from a picture of the tank seen from above, barrel up.

16 columns of 64x64: column n is the tank turned n * 22.5 degrees clockwise (column 4 faces right), like the columns of imgTanks.bmp (the game picks the
column from the tank's direction / 2). The background is the game's transparency colour (magenta).

Usage: tools/make_warlord_sprite.py <tank.png> [out.bmp]
"""
import math, sys
import numpy as np
from PIL import Image
from collections import deque

CELL = 64
RADIUS = 28.0          # the tank fits in a circle of this radius, so no turn clips it

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


def three_wheels(arr):
    """The picture has five or six blocks in each track: repaint each track as three round wheels (the yellow marks stay on the middle one).
    The coordinates are those of the picture this was made for (download.png, 1390x1065)."""
    from PIL import ImageDraw
    img = Image.fromarray(arr)
    for x0, x1, hx in ((480, 536, 498), (884, 931, 888)):
        y0, y1 = 458, 762
        patch = img.crop((hx, 600, hx + 34, 660))                     # the hazard marks
        d = ImageDraw.Draw(img)
        d.rectangle((x0 - 4, y0, x1 + 4, y1), fill=(10, 10, 14, 255))
        centres = [y0 + 50, (y0 + y1) // 2, y1 - 50]
        for cy in centres:
            box = (x0 - 6, cy - 46, x1 + 6, cy + 46)
            d.rounded_rectangle(box, radius=30, fill=(14, 14, 18, 255))                         # tyre outline
            d.rounded_rectangle((box[0] + 5, box[1] + 5, box[2] - 5, box[3] - 5), radius=26, fill=(104, 106, 116, 255))   # tyre
            d.rounded_rectangle((box[0] + 5, box[1] + 5, box[2] - 5, box[1] + 30), radius=22, fill=(170, 172, 184, 255))  # light from above
            cx = (box[0] + box[2]) // 2
            d.ellipse((cx - 17, cy - 17, cx + 17, cy + 17), fill=(205, 207, 216, 255), outline=(16, 16, 20, 255), width=4)   # hub
            d.ellipse((cx - 6, cy - 6, cx + 6, cy + 6), fill=(40, 40, 46, 255))
        img.paste(patch, (hx, 600 + 12), patch)
    return np.array(img)


WHEELS_SRC = [(x, y) for x in (508, 907) for y in (508, 610, 712)]   # centres of the six wheels in the picture
arr = three_wheels(arr)
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
TILT = 0.72            # the tank is seen from above and in front, like the others: the top is squashed ...
DEPTH = 4              # ... and the side of the hull shows below it (pixels)
for col in range(16):
    turned = canvas.rotate(-col * 22.5, resample=Image.BICUBIC)      # clockwise: column 4 faces right, as in imgTanks.bmp
    n = max(1, round(side * scale))
    top = turned.resize((n, max(1, round(n * TILT))), Image.LANCZOS)
    t = np.array(top).astype(np.float32)
    mask = t[:, :, 3] >= 140
    # light from the top left: a faint brightening of the upper-left half, a darkening of the lower-right
    yy, xx = np.mgrid[0:t.shape[0], 0:t.shape[1]]
    shade = 1.16 - 0.34 * ((xx / t.shape[1]) * 0.45 + (yy / t.shape[0]) * 0.55)
    rgbf = t[:, :, :3]
    # make it belong with the other tanks: the other sprites are rendered (soft black outline, muted metal, a little grain, bright specular)
    lum = rgbf @ np.array([0.30, 0.59, 0.11], np.float32)
    outline = np.clip((60.0 - lum) / 60.0, 0, 1)[:, :, None]                    # the heavy black line becomes dark warm grey
    rgbf = rgbf * (1 - 0.55 * outline) + np.array([58, 52, 58], np.float32) * 0.55 * outline
    lum = rgbf @ np.array([0.30, 0.59, 0.11], np.float32)
    rgbf = lum[:, :, None] + (rgbf - lum[:, :, None]) * 0.74                      # less saturated
    rgbf = rgbf * 0.88
    rgbf = rgbf + np.clip((lum[:, :, None] - 150) / 105.0, 0, 1) * 26.0         # specular on the pale parts and the dome
    rng = np.random.RandomState(7 + col)
    rgbf = rgbf + rng.randint(-7, 8, size=t.shape[:2])[:, :, None]               # grain
    lit = np.clip(rgbf * shade[:, :, None], 0, 255)
    # the side wall: the hull shifted down a pixel at a time, darker and bluer with depth
    H = t.shape[0] + DEPTH + 4
    layer = np.zeros((H, n, 3), np.float32)
    yy_full, xx_full = np.mgrid[0:H, 0:n].astype(np.float32)
    covered = np.zeros((H, n), bool)
    for d in range(DEPTH, 0, -1):
        wall = lit * (0.74 - 0.03 * (DEPTH - d)) + np.array([4, 6, 14], np.float32)
        sl = (slice(d, d + t.shape[0]), slice(0, n))
        layer[sl][mask] = wall[mask]
        covered[sl] |= mask
    # the wheels: round black tyres with a pale hub hanging on the side of the hull, like the other tanks. Placed where the picture's wheels
    # are, turned with the tank, squashed with the tilt, and dropped towards the bottom of the side wall.
    th = -math.radians(col * 22.5)
    for (wx, wy) in WHEELS_SRC:
        px_, py_ = wx - xs.min() + (side - w) // 2 - side / 2, wy - ys.min() + (side - h) // 2 - side / 2
        rx, ry = px_ * math.cos(th) + py_ * math.sin(th), -px_ * math.sin(th) + py_ * math.cos(th)    # anticlockwise, like Image.rotate
        cx, cy = n / 2 + rx * scale, t.shape[0] / 2 + ry * scale * TILT + DEPTH * 0.9
        R = 5.0
        d2 = (xx_full - cx) ** 2 + (yy_full - cy) ** 2
        tyre = d2 <= R * R
        layer[tyre] = (18, 18, 22)
        covered |= tyre
        rim = (d2 <= (R - 1.2) ** 2) & (d2 > (R - 2.4) ** 2) & ((xx_full - cx) < 1)       # a pale edge on the lit side
        layer[rim] = (110, 112, 122)
        hub = d2 <= 2.6 ** 2
        layer[hub] = (196, 198, 208)
        layer[d2 <= 1.0] = (60, 60, 68)
    sl = (slice(0, t.shape[0]), slice(0, n))
    layer[sl][mask] = lit[mask]
    covered[sl] |= mask
    rgb = np.where(covered[:, :, None], layer, np.array([255, 0, 255], np.float32)).astype(np.uint8)
    img = Image.fromarray(rgb)
    sheet.paste(img, (col * CELL + (CELL - n) // 2, (CELL - H) // 2))
sheet.save(out_path)
print("wrote", out_path, sheet.size)

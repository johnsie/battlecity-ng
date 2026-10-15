#!/usr/bin/env python3
"""Generates data/imgFont.bmp: a 16x14 grid of 8x16 glyphs (Latin-1, chars 32..255), white on black.
The original client used the Windows SYSTEM_FIXED_FONT (8x16); this reproduces that cell size on every
platform. Source font: Liberation Mono (SIL Open Font License 1.1).
Usage: tools/make_font_atlas.py [font.ttf] [out.bmp]"""
import sys
from PIL import Image, ImageDraw, ImageFont

font_path = sys.argv[1] if len(sys.argv) > 1 else "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf"
out = sys.argv[2] if len(sys.argv) > 2 else "data/imgFont.bmp"
CW, CH, COLS = 8, 16, 16
font = ImageFont.truetype(font_path, 13)
img = Image.new("1", (CW * COLS, CH * 14), 0)
d = ImageDraw.Draw(img)
d.fontmode = "1"  # no antialiasing: crisp pixels like GDI's fixed font
for code in range(32, 256):
    i = code - 32
    x, y = (i % COLS) * CW, (i // COLS) * CH
    d.text((x, y + 1), chr(code), font=font, fill=1)
img.convert("RGB").save(out)
print("wrote", out, img.size)

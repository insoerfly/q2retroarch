#!/usr/bin/env python3
"""gen_cover.py - draw simple placeholder box-art covers for testing.

Writes assets/covers/<name>.png (reference) and <name>.565 (device format:
"C565" + u16 width + u16 height + width*height little-endian RGB565)."""
import os
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))   # launcher/tools
ROOT = os.path.dirname(HERE)                        # launcher/
OUT  = os.path.join(ROOT, "assets", "covers")

GAMES = ["Battle_City", "Contra", "Crash_Bandicoot", "Dragon_Ball",
         "Final_Fight_One", "Metal_Slug", "Street_Fighter", "Teenage_Mutant",
         "Tekken_Advance", "The_King_of_Fighters", "World_Soccer"]

PALETTES = [((210, 70, 60), (30, 20, 60)), ((40, 90, 200), (10, 20, 50)),
            ((220, 170, 40), (60, 30, 20)), ((60, 180, 120), (15, 40, 45)),
            ((170, 60, 200), (30, 15, 50))]

W, H = 128, 176


def font(sz):
    for f in (os.path.join(ROOT, "xmb.ttf"),
              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if os.path.exists(f):
            return ImageFont.truetype(f, sz)
    return ImageFont.load_default()


def make(name, idx):
    c1, c2 = PALETTES[idx % len(PALETTES)]
    im = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(im)
    for y in range(H):
        t = y / H
        d.line([(0, y), (W, y)], fill=tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3)))
    d.rectangle([6, 6, W - 7, H - 7], outline=(255, 255, 255), width=2)
    d.ellipse([W // 2 - 34, 24, W // 2 + 34, 92], fill=(255, 220, 90))
    d.ellipse([W // 2 - 22, 40, W // 2 + 22, 84], fill=c2)
    d.rectangle([10, H - 46, W - 11, H - 10], fill=(0, 0, 0))
    d.text((14, H - 42), name.replace("_", " ")[:16], font=font(13), fill=(255, 255, 255))

    im.save(os.path.join(OUT, name + ".png"))

    px = im.load()
    buf = bytearray(b"C565" + bytes([W & 0xFF, W >> 8, H & 0xFF, H >> 8]))
    for y in range(H):
        for x in range(W):
            r, g, b = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            buf += bytes([v & 0xFF, v >> 8])
    with open(os.path.join(OUT, name + ".565"), "wb") as f:
        f.write(buf)


os.makedirs(OUT, exist_ok=True)
for i, g in enumerate(GAMES):
    make(g, i)
print("wrote %d covers to %s" % (len(GAMES), OUT))

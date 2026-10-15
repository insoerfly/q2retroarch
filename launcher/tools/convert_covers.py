#!/usr/bin/env python3
"""convert_covers.py - turn images in ./lab into launcher covers.

For each ./lab/<name>.<ext> writes:
  assets/covers/<name>.png   (thumbnail, for previews)
  assets/covers/<name>.565   (device format: "C565" + u16 w + u16 h + RGB565 LE)
Cover name == game name (ROM file name without extension)."""
import os, glob
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))   # launcher/tools
ROOT = os.path.dirname(HERE)                        # launcher/
LAB  = os.path.join(HERE, "lab")                    # launcher/tools/lab
OUT  = os.path.join(ROOT, "assets", "covers")
MAXW, MAXH = 160, 224          # thumbnail cap ("not full quality")

os.makedirs(OUT, exist_ok=True)


def write565(im, path):
    w, h = im.size
    px = im.load()
    buf = bytearray(b"C565" + bytes([w & 0xFF, w >> 8, h & 0xFF, h >> 8]))
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            buf += bytes([v & 0xFF, v >> 8])
    open(path, "wb").write(buf)


n = 0
files = sorted(glob.glob(os.path.join(LAB, "*")))
print("lab files:", len(files))
for p in files:
    name = os.path.splitext(os.path.basename(p))[0]
    try:
        im = Image.open(p).convert("RGB")
    except Exception as e:
        print("open failed:", os.path.basename(p), "->", repr(e))
        continue
    im.thumbnail((MAXW, MAXH), Image.LANCZOS)
    im.save(os.path.join(OUT, name + ".png"))
    write565(im, os.path.join(OUT, name + ".565"))
    n += 1
print("converted %d covers -> %s (%dx%d thumbnails)" % (n, OUT, MAXW, MAXH))

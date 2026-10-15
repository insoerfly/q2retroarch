#!/usr/bin/env python3
"""fetch_covers.py - grab a random photo per game (placedog.net) and compose a
cover-looking image: photo + title bar with the game name. Saved to ./lab so
convert_covers.py turns them into <name>.png / <name>.565.

(Placeholder imagery for testing - not real game art.)"""
import os, io, glob, urllib.request
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))   # launcher/tools
ROOT = os.path.dirname(HERE)                        # launcher/
LAB  = os.path.join(HERE, "lab")                    # launcher/tools/lab
GAMES = ["Battle_City", "Contra", "Crash_Bandicoot", "Dragon_Ball",
         "Final_Fight_One", "Metal_Slug", "Street_Fighter", "Teenage_Mutant",
         "Tekken_Advance", "The_King_of_Fighters", "World_Soccer"]
W, H = 160, 224

os.makedirs(LAB, exist_ok=True)
for p in glob.glob(os.path.join(LAB, "*")):
    os.remove(p)


def font(sz):
    for f in (os.path.join(ROOT, "xmb.ttf"),
              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if os.path.exists(f):
            return ImageFont.truetype(f, sz)
    return ImageFont.load_default()


def fallback(idx):
    im = Image.new("RGB", (W, H))
    d = ImageDraw.Draw(im)
    for y in range(H):
        t = y / H
        d.line([(0, y), (W, y)],
               fill=(int(60 + 120 * t), int(40 + 90 * (1 - t)), int(120 - 60 * t)))
    return im


for i, g in enumerate(GAMES):
    im = None
    url = "https://placedog.net/300/400?random=%d" % i
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        data = urllib.request.urlopen(req, timeout=40).read()
        im = Image.open(io.BytesIO(data)).convert("RGB").resize((W, H), Image.LANCZOS)
        print("ok  ", g, len(data))
    except Exception as e:
        print("fallback", g, repr(e))
        im = fallback(i)
    d = ImageDraw.Draw(im)
    d.rectangle([0, H - 30, W, H], fill=(0, 0, 0))
    d.text((6, H - 27), g.replace("_", " "), font=font(13), fill=(255, 255, 255))
    im.save(os.path.join(LAB, g + ".png"))

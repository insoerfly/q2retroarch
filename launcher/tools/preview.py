#!/usr/bin/env python3
"""preview.py - render a preview PNG of the XMB launcher layout (design review)."""
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

HERE   = os.path.dirname(os.path.abspath(__file__))   # launcher/tools
ROOT   = os.path.dirname(HERE)                        # launcher/
RA     = os.path.join(ROOT, "assets", "ra")
OUTDIR = os.path.join(ROOT, "preview")
SCR_W, SCR_H = 480, 272

# Flags: SPACED=1 (spaces in game titles), SETTINGS=1 (settings overlay),
#        APPEARANCE=1 (Appearance submenu), GROUPED=1 (games grouped by system),
#        THEME=n (theme index, default 0)
SPACED     = os.environ.get("SPACED") == "1"
SETTINGS   = os.environ.get("SETTINGS") == "1"
APPEARANCE = os.environ.get("APPEARANCE") == "1"
GROUPED    = os.environ.get("GROUPED") == "1"
COVERS     = os.environ.get("COVERS") == "1"      # flat games + cover pane
GCOVERS    = os.environ.get("GCOVERS") == "1"     # grouped games + cover pane
THEME      = int(os.environ.get("THEME", "0"))

if   APPEARANCE: OUTNAME = "preview_appearance.png"
elif GCOVERS:    OUTNAME = "preview_grouped_covers.png"
elif GROUPED:    OUTNAME = "preview_grouped.png"
elif COVERS:     OUTNAME = "preview_covers.png"
elif SETTINGS:   OUTNAME = "preview_settings.png"
elif SPACED:     OUTNAME = "preview_spaces.png"
else:            OUTNAME = "preview.png"

UI_LANG = os.environ.get("UI_LANG", "ru")     # ru (default) | en
if UI_LANG == "en":
    OUTNAME = OUTNAME.replace(".png", "_en.png")

UI_FONT = os.environ.get("UI_FONT", "")       # optional font file (name in ROOT or a path)
if UI_FONT:
    tag = os.path.splitext(os.path.basename(UI_FONT))[0].replace(" ", "_")
    OUTNAME = OUTNAME.replace(".png", "_" + tag + ".png")


def L(en, ru):
    return ru if UI_LANG == "ru" else en

# name, bg_top, bg_bot, wave_l, wave_r, glow, bar
THEMES=[("Blue",  (0x0A,0x0E,0x1A),(0x1A,0x1F,0x3A),(0x1C,0x42,0x76),(0x4A,0x2E,0x7A),(0x15,0x24,0x3E),(0x1E,0x3E,0x66)),
        ("Teal",  (0x06,0x14,0x0F),(0x12,0x30,0x28),(0x1B,0x6B,0x57),(0x2E,0x6A,0x7A),(0x12,0x35,0x28),(0x1E,0x5A,0x4A)),
        ("Sunset",(0x1A,0x0A,0x0E),(0x3A,0x1A,0x1F),(0x76,0x34,0x1C),(0x7A,0x2E,0x4A),(0x3E,0x1A,0x24),(0x5A,0x2E,0x1E)),
        ("Mono",  (0x10,0x10,0x14),(0x24,0x24,0x2C),(0x3A,0x3A,0x46),(0x56,0x56,0x66),(0x24,0x24,0x2C),(0x3A,0x3A,0x46))]
_t = THEMES[THEME % len(THEMES)]
C_BG_TOP,C_BG_BOT,C_WAVE_L,C_WAVE_R,C_GLOW,C_BAR_M = _t[1:7]
C_CAT_SEL=(255,255,255); C_CAT_DIM=(0x8A,0x98,0xB8)
C_ITEM_SEL=(255,255,255); C_ITEM_DIM=(0x8A,0x96,0xB4)
C_HINT=(0x6E,0x7E,0x9E)
CAT_DIM=(0x96,0xA2,0xC4)

NES=["Battle_City","Contra"]
GBA=["Crash_Bandicoot","Dragon_Ball","Final_Fight_One","Metal_Slug",
     "Street_Fighter","Teenage_Mutant","Tekken_Advance",
     "The_King_of_Fighters","World_Soccer"]
GAME_ALL=NES+GBA
APPS_LIST=[L("Calculator","Калькулятор"), L("Calendar","Календарь"),
           L("eBook","Читалка"), L("Games","Игры"), L("Music","Музыка"),
           L("Photos","Фото"), L("Recorder","Диктофон"), L("Settings","Настройки"),
           L("Videos","Видео"), L("Stopwatch","Секундомер")]

CATS=[("image",L("Photo","Фото"),[L("All Photos","Все фото"),L("Camera","Камера")]),
      ("music",L("Music","Музыка"),[L("Music","Музыка")]),
      ("menu_video",L("Video","Видео"),[L("Videos","Видео")]),
      ("core",L("Game","Игры"),GAME_ALL),
      ("folder",L("Apps","Приложения"),APPS_LIST),
      ("setting",L("Setting","Настройки"),[L("Settings","Настройки")])]
SEL=3; ITEMSEL=2
# grouped view: vertical emulator submenu (left) + games pane (right)
EMU=[("NES",NES),("GBA",GBA),("SNES",[]),("Genesis",[])]
ESUB=1; GSEL=2; GFOCUS=1


def font(sz):
    cands = []
    if UI_FONT:
        cands.append(UI_FONT if os.path.isabs(UI_FONT) else os.path.join(ROOT, UI_FONT))
    cands += [os.path.join(ROOT, "ui.ttf"), os.path.join(ROOT, "xmb.ttf"),
              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"]
    for f in cands:
        if os.path.exists(f):
            return ImageFont.truetype(f, sz)
    return ImageFont.load_default()


def mix(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def fit(d, s, wmax, fnt):
    if d.textlength(s, font=fnt) <= wmax:
        return s
    while len(s) > 3 and d.textlength(s + "...", font=fnt) > wmax:
        s = s[:-1]
    return s + "..."


def in_rrect(px, py, x0, y0, x1, y1, r):
    if px < x0 or px > x1 or py < y0 or py > y1:
        return False
    if r <= 0:
        return True
    dx = dy = 0
    if px < x0 + r:
        dx = (x0 + r) - px
    elif px > x1 - r:
        dx = px - (x1 - r)
    if py < y0 + r:
        dy = (y0 + r) - py
    elif py > y1 - r:
        dy = py - (y1 - r)
    if dx == 0 or dy == 0:
        return True
    return dx * dx + dy * dy <= r * r


def selbox(d, x0, y0, x1, y1, t=2, r=3):
    """selection highlight: 4px rounded outline in a theme-derived colour."""
    if x1 - x0 < 2 * t + 4 or y1 - y0 < 2 * t + 4:
        return
    ir = max(1, r - t)
    col = mix(C_BAR_M, (255, 255, 255), 120 / 255)
    ov = Image.new("RGBA", img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(ov)
    for yy in range(y0, y1 + 1):
        for xx in range(x0, x1 + 1):
            if in_rrect(xx, yy, x0, y0, x1, y1, r) and \
               not in_rrect(xx, yy, x0 + t, y0 + t, x1 - t, y1 - t, ir):
                od.point((xx, yy), fill=col + (230,))
    img.paste(ov, (0, 0), ov)


def draw_cover(d, name, fx0, fy0, fx1, fy1):
    """box-art preview (assets/covers/<name>.png): blurred backdrop + aspect-fit, no frame."""
    if fx1 - fx0 < 10 or fy1 - fy0 < 10:
        return
    fw, fh = fx1 - fx0, fy1 - fy0
    p = os.path.join(ROOT, "assets", "covers", name + ".png")
    if os.path.exists(p):
        cov = Image.open(p).convert("RGB")
        # background: stretched + blurred + darkened (fills bars if aspect differs)
        bg = cov.resize((fw, fh), Image.LANCZOS).filter(ImageFilter.GaussianBlur(7))
        bg = Image.eval(bg, lambda v: int(v * 0.55))
        img.paste(bg, (fx0, fy0))
        # foreground: scaled to fit (contain); 1:1 when the aspect matches
        r = min(fw / cov.width, fh / cov.height)
        dw, dh = max(1, int(cov.width * r)), max(1, int(cov.height * r))
        img.paste(cov.resize((dw, dh), Image.LANCZOS),
                  (fx0 + (fw - dw) // 2, fy0 + (fh - dh) // 2))
    else:
        pt = mix(C_BG_TOP, (255, 255, 255), 40 / 255)
        pb = mix(C_BG_BOT, (255, 255, 255), 40 / 255)
        for y in range(fy0, fy1):
            d.line([(fx0, y), (fx1, y)], fill=mix(pt, pb, (y - fy0) / (fy1 - fy0)))
        d.text((fx0 + 4, (fy0 + fy1) // 2 - 6), name, font=font(12), fill=C_HINT)


def icon(name, size, color):
    base = Image.open(os.path.join(RA, name + ".png")).convert("RGBA").resize((size, size), Image.LANCZOS)
    ic = Image.new("RGBA", base.size, color)
    ic.putalpha(base.split()[3])
    return ic


def bg(y):
    return mix(C_BG_TOP, C_BG_BOT, y / SCR_H)


img = Image.new("RGB", (SCR_W, SCR_H))
d = ImageDraw.Draw(img)
for y in range(SCR_H):
    d.line([(0, y), (SCR_W, y)], fill=bg(y))


def wave(yb, amp, ph):
    lay = Image.new("RGBA", (SCR_W, SCR_H), (0, 0, 0, 0))
    wd = ImageDraw.Draw(lay)
    prev = None
    for x in range(0, SCR_W + 1, 3):
        y = yb + int(amp * math.sin(x * 2 * math.pi / SCR_W + math.radians(ph)))
        c = mix(C_WAVE_L, C_WAVE_R, x / SCR_W)
        if prev:
            wd.line([prev, (x, y)], fill=c + (140,), width=2)
        prev = (x, y)
    lay = lay.filter(ImageFilter.GaussianBlur(1.6))
    img.paste(lay, (0, 0), lay)


wave(150, 40, 0); wave(150, 32, 60); wave(150, 24, 120)

slot = 150; cx = SCR_W // 2
for i, (name, label, _) in enumerate(CATS):
    x = cx + (i - SEL) * slot
    if x + 70 < 0 or x - 70 > SCR_W:
        continue
    sel = (i == SEL)
    size = 46 if sel else 40
    if sel:
        halo = Image.new("RGBA", (SCR_W, SCR_H), (0, 0, 0, 0))
        ImageDraw.Draw(halo).ellipse([x - 38, 60 - 38, x + 38, 60 + 38], fill=C_GLOW + (70,))
        halo = halo.filter(ImageFilter.GaussianBlur(9))
        img.paste(halo, (0, 0), halo)
    ic = icon(name, size, (255, 255, 255) if sel else CAT_DIM)
    img.paste(ic, (x - size // 2, 60 - size // 2), ic)
    fc = font(20 if sel else 16)
    d.text((x - d.textlength(label, font=fc) / 2, 104), label, font=fc,
           fill=(C_CAT_SEL if sel else C_CAT_DIM))

EDGE = mix(C_BG_BOT, (255, 255, 255), 110 / 255)

if GROUPED or GCOVERS:
    y0, lh = 150, 21
    rows = (SCR_H - 8 - y0) // lh
    if GCOVERS:
        lx0, lx1, div1 = 16, 96, 112
        rx0, rx1, div2, cov_x = 122, 332, 348, 358
    else:
        lx0, lx1 = 56, 168
        rx0, rx1 = 200, SCR_W - 12
        div1 = (lx1 + rx0) // 2
        div2 = None
    for i, (name, roms) in enumerate(EMU):
        iy = y0 + i * lh
        on = (i == ESUB)
        if on:
            selbox(d, 8, iy - 1, div1 - 8, iy + lh - 1)
        d.text((lx0, iy + 3), name, font=font(14),
               fill=(C_ITEM_SEL if on else C_ITEM_DIM))
    dy1 = min(y0 + rows * lh - 2, SCR_H - 20)
    s = dy1 - 6 - y0
    l = s * 7 // 30
    dtop = y0 + (s - l) // 2
    dbot = dtop + l
    ov = Image.new("RGBA", img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(ov)
    od.line([(div1, dtop), (div1, dbot)], fill=EDGE + (128,))
    if div2:
        od.line([(div2, dtop), (div2, dbot)], fill=EDGE + (128,))
    img.paste(ov, (0, 0), ov)
    gitems = EMU[ESUB][1]
    n = len(gitems)
    top = max(0, min(GSEL - rows // 2, n - rows)) if n else 0
    last = min(top + rows, n)
    if n == 0:
        d.text((rx0, y0 + 3), "(no games)", font=font(14), fill=C_HINT)
    gx1 = div2 if GCOVERS else SCR_W
    gx0 = max(rx0 - 8, div1 + 3)           # 8px from text, >=3px off the divider
    for i in range(top, last):
        iy = y0 + (i - top) * lh
        bright = (GFOCUS == 1) and (i == GSEL)
        if bright:
            selbox(d, gx0, iy - 1, gx1 - 8, iy + lh - 1)
        d.text((rx0, iy + 3), fit(d, gitems[i], rx1 - rx0, font(14)), font=font(14),
               fill=(C_ITEM_SEL if bright else C_ITEM_DIM))
    if GCOVERS and n > 0:
        sc = min(dy1 - (y0 - 6), (SCR_W - 8) - cov_x)
        cx = cov_x + ((SCR_W - 8) - cov_x - sc) // 2
        draw_cover(d, gitems[GSEL], cx, y0 - 6, cx + sc, y0 - 6 + sc)
    hint = (L("OK - Play   Back - Up", "OK - Играть   Назад - Вверх") if GFOCUS == 1
            else L("OK - Open   Menu - Settings   Back - Exit",
                   "OK - Открыть   Меню - Настройки   Назад - Выход"))
elif COVERS:
    y0, lh = 150, 21
    x0, x1, div = 56, 222, 240
    cov0, cov1 = 250, SCR_W - 48
    items = CATS[SEL][2]
    rows = (SCR_H - 8 - y0) // lh
    top = max(0, min(ITEMSEL - rows // 2, len(items) - rows))
    last = min(top + rows, len(items))
    dy1 = min(y0 + rows * lh - 2, SCR_H - 20)
    s = dy1 - 6 - y0
    l = s * 7 // 30
    dtop = y0 + (s - l) // 2
    ov = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(ov).line([(div, dtop), (div, dtop + l)], fill=EDGE + (128,))
    img.paste(ov, (0, 0), ov)
    for i in range(top, last):
        it = items[i]
        iy = y0 + (i - top) * lh
        if i == ITEMSEL:
            selbox(d, 48, iy - 1, div - 8, iy + lh - 1)
        d.text((x0, iy + 3), fit(d, it, x1 - x0, font(14)), font=font(14),
               fill=(C_ITEM_SEL if i == ITEMSEL else C_ITEM_DIM))
    if top > 0:
        d.text((x0, y0 - 12), "...", font=font(12), fill=C_HINT)
    if last < len(items):
        d.text((x0, y0 + rows * lh + 1), "...", font=font(12), fill=C_HINT)
    if items:
        draw_cover(d, items[ITEMSEL], cov0, y0 - 6, cov1, dy1)
    hint = L("OK - Select   Menu - Settings   Back - Exit",
             "OK - Выбрать   Меню - Настройки   Назад - Выход")
else:
    x0, y0, lh = 56, 150, 21
    x1 = x0 + 320
    bx0 = x0 - 10; bspan = SCR_W - bx0
    items = CATS[SEL][2]
    rows = (SCR_H - 8 - y0) // lh
    top = max(0, min(ITEMSEL - rows // 2, len(items) - rows))
    last = min(top + rows, len(items))
    for i in range(top, last):
        it = items[i].replace("_", " ") if SPACED else items[i]
        iy = y0 + (i - top) * lh
        if i == ITEMSEL:
            selbox(d, 48, iy - 1, SCR_W - 48, iy + lh - 1)
        d.text((x0, iy + 3), fit(d, it, x1 - x0, font(14)), font=font(14),
               fill=(C_ITEM_SEL if i == ITEMSEL else C_ITEM_DIM))
    if top > 0:
        d.text((x0, y0 - 12), "...", font=font(12), fill=C_HINT)
    if last < len(items):
        d.text((x0, y0 + rows * lh + 1), "...", font=font(12), fill=C_HINT)
    hint = L("OK - Select   Menu - Settings   Back - Exit",
             "OK - Выбрать   Меню - Настройки   Назад - Выход")

d.text((SCR_W - 12 - d.textlength(hint, font=font(12)), SCR_H - 16), hint, font=font(12), fill=C_HINT)

if SETTINGS or APPEARANCE:
    pt = mix(C_BG_TOP, (255, 255, 255), 40 / 255)
    pb = mix(C_BG_BOT, (255, 255, 255), 40 / 255)
    edge = mix(C_BG_BOT, (255, 255, 255), 110 / 255)
    px0, py0, px1, py1 = 96, 40, SCR_W - 96, SCR_H - 40
    for y in range(py0, py1 + 1):
        d.line([(px0, y), (px1, y)], fill=mix(pt, pb, (y - py0) / (py1 - py0)))
    for (xa, ya, xb, yb) in [(px0, py0, px1, py0), (px0, py1, px1, py1),
                             (px0, py0, px0, py1), (px1, py0, px1, py1)]:
        d.line([(xa, ya), (xb, yb)], fill=edge)
    THEME_RU = {"Blue": "Синий", "Teal": "Бирюзовый", "Sunset": "Закат", "Mono": "Моно"}
    if APPEARANCE:
        title = L("Appearance - background", "Оформление - фон")
        items = [L(t[0], THEME_RU[t[0]]) for t in THEMES]
        sel = THEME % len(THEMES)
    else:
        title = L("Settings", "Настройки")
        items = [L("Appearance", "Оформление"), L("Language", "Язык"),
                 L("Group games: Off", "Группировка игр: Выкл"),
                 L("Covers: Off", "Обложки: Выкл"),
                 L("Reboot", "Перезагрузка"), L("Power off", "Выключение")]
        sel = 0
    d.text((px0 + 14, py0 + 8), title, font=font(16), fill=(255, 255, 255))
    for i, it in enumerate(items):
        iy = py0 + 36 + i * 21
        if i == sel:
            for y in range(iy, iy + 18):
                d.line([(px0 + 8, y), (px1 - 8, y)],
                       fill=mix(mix(pt, pb, (y - py0) / (py1 - py0)), C_BAR_M, 0.59))
        d.text((px0 + 16, iy + 2), it, font=font(14),
               fill=(255, 255, 255) if i == sel else (0x8A, 0x96, 0xB4))
    d.text((px0 + 16, py1 - 20), L("OK - Apply   Back - Close", "OK - Применить   Назад - Закрыть"),
           font=font(12), fill=C_HINT)

os.makedirs(OUTDIR, exist_ok=True)
out = os.path.join(OUTDIR, OUTNAME)
tmp = os.path.join(OUTDIR, "preview_tmp.png")
img.save(tmp)
try:
    os.replace(tmp, out)
    print("wrote", out)
except OSError:
    print("wrote", tmp, "(target busy)")

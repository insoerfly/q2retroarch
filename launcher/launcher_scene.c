/*
 * launcher_scene.c - RetroArch/PSP-style XMB (Cross Media Bar) launcher scene.
 *
 * Static XMB (no animations) drawn with the low-level GUI/GDI API.
 * Visuals follow a review list: bigger icon+label unit, emphasis of the active
 * category (bigger/brighter + soft disc), left-aligned item list with a soft
 * gradient bar, wide gradient waves, blue-violet palette, small hints bottom-right.
 *
 * Not part of the atj2273sdk repo (separate project).
 */
#include "launcher.h"
#include <key.h>
#include <gui/wm_inc.h>
#include <applib/msg_loop.h>   /* send_async_msg / msg_apps_t */
#include <app_define.h>        /* APP_NAME_MANAGER */
#include <dirent.h>            /* opendir/readdir for the SD scan */
#include <stat.h>              /* stat / S_ISDIR */
#include <fcntl.h>             /* open / O_RDWR (reboot via watchdog) */
#include <sys/ioctl.h>         /* ioctl (reboot via watchdog) */
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "xmb_assets.h"   /* RetroArch XMB icons (CC-BY-4.0), RGB565A */

#define APPS "/mnt/diska/apps"

#define SCR_W   480
#define SCR_H   272

/* ---- i18n (language 0 = Russian [default], 1 = English) -------------------- */
enum {
    T_PHOTO, T_MUSIC, T_VIDEO, T_GAME, T_APPS, T_SETTING,
    T_ALLPHOTOS, T_CAMERA, T_MUSIC1, T_VIDEOS, T_SETTINGS, T_NOGAMES, T_GAMES,
    T_HINT_MAIN, T_HINT_GAME, T_HINT_OPEN, T_HINT_SET,
    T_SET_TITLE, T_APPEARANCE, T_GROUP_ON, T_GROUP_OFF,
    T_COVERS_ON, T_COVERS_OFF, T_LANGUAGE, T_REBOOT, T_POWEROFF,
    T_APPEAR_TITLE, T_BLUE, T_TEAL, T_SUNSET, T_MONO,
    T_LANG_TITLE, T_RUSSIAN, T_ENGLISH,
    T_NSTR
};
typedef struct { const char *en; const char *ru; } str_t;
static const str_t s_str[T_NSTR] = {
    [T_PHOTO]       = { "Photo",      "Фото" },
    [T_MUSIC]       = { "Music",      "Музыка" },
    [T_VIDEO]       = { "Video",      "Видео" },
    [T_GAME]        = { "Game",       "Игры" },
    [T_APPS]        = { "Apps",       "Приложения" },
    [T_SETTING]     = { "Setting",    "Настройки" },
    [T_ALLPHOTOS]   = { "All Photos", "Все фото" },
    [T_CAMERA]      = { "Camera",     "Камера" },
    [T_MUSIC1]      = { "Music",      "Музыка" },
    [T_VIDEOS]      = { "Videos",     "Видео" },
    [T_SETTINGS]    = { "Settings",   "Настройки" },
    [T_NOGAMES]     = { "(no games)", "(нет игр)" },
    [T_GAMES]       = { "Games",      "Игры" },
    [T_HINT_MAIN]   = { "OK - Select   Menu - Settings   Back - Exit",
                        "OK - Выбрать   Меню - Настройки   Назад - Выход" },
    [T_HINT_GAME]   = { "OK - Play   Back - Up", "OK - Играть   Назад - Вверх" },
    [T_HINT_OPEN]   = { "OK - Open   Menu - Settings   Back - Exit",
                        "OK - Открыть   Меню - Настройки   Назад - Выход" },
    [T_HINT_SET]    = { "OK - Apply   Back - Close", "OK - Применить   Назад - Закрыть" },
    [T_SET_TITLE]   = { "Settings",   "Настройки" },
    [T_APPEARANCE]  = { "Appearance", "Оформление" },
    [T_GROUP_ON]    = { "Group games: On",  "Группировка игр: Вкл" },
    [T_GROUP_OFF]   = { "Group games: Off", "Группировка игр: Выкл" },
    [T_COVERS_ON]   = { "Covers: On",  "Обложки: Вкл" },
    [T_COVERS_OFF]  = { "Covers: Off", "Обложки: Выкл" },
    [T_LANGUAGE]    = { "Language",    "Язык" },
    [T_REBOOT]      = { "Reboot",      "Перезагрузка" },
    [T_POWEROFF]    = { "Power off",   "Выключение" },
    [T_APPEAR_TITLE]= { "Appearance - background", "Оформление - фон" },
    [T_BLUE]        = { "Blue",   "Синий" },
    [T_TEAL]        = { "Teal",   "Бирюзовый" },
    [T_SUNSET]      = { "Sunset", "Закат" },
    [T_MONO]        = { "Mono",   "Моно" },
    [T_LANG_TITLE]  = { "Language", "Язык" },
    [T_RUSSIAN]     = { "Русский", "Русский" },
    [T_ENGLISH]     = { "English", "English" },
};
#define T_(id) (s_lang == 0 ? s_str[id].ru : s_str[id].en)

/* ---- model ----------------------------------------------------------------- */
#define MAX_CATS  8
#define MAX_ITEMS 16

typedef struct
{
    const char *name;
    int         n;
    int         icon;             /* index into the category icon arrays */
    const char *items[MAX_ITEMS];
    const char *app[MAX_ITEMS];   /* target .app path (NULL = nothing) */
    const char *arg[MAX_ITEMS];   /* optional argument string          */
} xmb_cat_t;

static xmb_cat_t s_cats[MAX_CATS];
static int       s_ncat = 0;

/* ---- colours (0xRRGGBB) ---------------------------------------------------- */
#define C_CAT_SEL   0xFFFFFF
#define C_CAT_DIM   0x7A88A8
#define C_ITEM_SEL  0xFFFFFF
#define C_ITEM_DIM  0x8A96B4
#define C_HINT      0x6E7E9E

/* themeable colours (set by _apply_theme); the settings panel follows the theme */
static int C_BG_TOP, C_BG_BOT, C_WAVE_L, C_WAVE_R, C_GLOW, C_BAR_M;
static int C_PANEL_T, C_PANEL_B, C_PANEL_EDGE;

typedef struct { const char *name; int top, bot, wl, wr, glow, bar; } theme_t;
static const theme_t s_themes[] = {
    { "Blue",   0x0A0E1A, 0x1A1F3A, 0x1C4276, 0x4A2E7A, 0x15243E, 0x1E3E66 },
    { "Teal",   0x06140F, 0x123028, 0x1B6B57, 0x2E6A7A, 0x123528, 0x1E5A4A },
    { "Sunset", 0x1A0A0E, 0x3A1A1F, 0x76341C, 0x7A2E4A, 0x3E1A24, 0x5A2E1E },
    { "Mono",   0x101014, 0x24242C, 0x3A3A46, 0x565666, 0x24242C, 0x3A3A46 },
};
#define NTHEMES ((int)(sizeof(s_themes) / sizeof(s_themes[0])))
static int s_theme = 0;

/* ---- settings state -------------------------------------------------------- */
#define CFG_PATH APPS "/launcher/launcher.cfg"
static bool s_settings = false;   /* overlay visible            */
static int  s_set_page = 0;       /* 0 = main, 1 = appearance   */
static int  s_set_sel  = 0;
static bool s_group_emu = false;  /* group Game by emulator     */
static bool s_cov_on = false;     /* show box-art covers        */
static int  s_lang = 0;           /* 0 = Russian (default), 1 = English */

/* ---- state ----------------------------------------------------------------- */
static int  s_win = -1;
static int  s_hdc = -1;
static int  s_font = -1;
static int  s_cat = 0;
static int  s_item = 0;
static bool s_exit = false;

/* emulator grouping: vertical submenu (left) + games pane (right) */
static const char *s_emu_mode[4] = { "NES", "GBC", "SFC", "MD" };
static const char *s_emu_name[4] = { "NES", "GBA", "SNES", "Genesis" };
#define MAX_GITEMS 32
static char s_glab[4][MAX_GITEMS][64];   /* game label            */
static char s_garg[4][MAX_GITEMS][96];   /* emulator args         */
static int  s_gn[4];                     /* games per emulator    */
static int  s_cat_game = -1;             /* index of the Game cat */
static int  s_sub = 0;                   /* selected emulator     */
static int  s_gsel = 0;                  /* selected game         */
static int  s_focus = 0;                 /* 0 emulators, 1 games  */

/* ---- helpers --------------------------------------------------------------- */
static int _lerp(int a, int b, int t, int n) { return a + (b - a) * t / n; }
static int _mix(int c1, int c2, int t, int n)
{
    int r = _lerp((c1 >> 16) & 0xFF, (c2 >> 16) & 0xFF, t, n);
    int g = _lerp((c1 >> 8) & 0xFF,  (c2 >> 8) & 0xFF,  t, n);
    int b = _lerp(c1 & 0xFF,          c2 & 0xFF,         t, n);
    return (r << 16) | (g << 8) | b;
}

static void _fill_rect(int hdc, int x0, int y0, int x1, int y1, int color)
{
    int y;
    gui_dc_set_color(hdc, color);
    for (y = y0; y <= y1; y++)
        gui_dc_draw_line(hdc, x0, y, x1, y);
}

/* soft filled disc (for active-category spotlight) */
static void _fill_disc(int hdc, int cx, int cy, int r, int color)
{
    int dy;
    gui_dc_set_color(hdc, color);
    for (dy = -r; dy <= r; dy++)
    {
        int w = r * r - dy * dy;
        int hw = 0;
        while ((hw + 1) * (hw + 1) <= w) hw++;
        gui_dc_draw_line(hdc, cx - hw, cy + dy, cx + hw, cy + dy);
    }
}

/* integer sine ~ -256..256 */
static int _isin(int deg)
{
    static const int tab[25] = { 0,66,131,191,242,284,316,338,350,352,346,331,
                                  308,278,242,202,158,113,66,22,-22,-66,-113,-158,-202 };
    int d = ((deg % 360) + 360) % 360;
    int i = d / 15;
    if (i >= 24) return 0;
    return tab[i];
}

/* soft radial halo: filled discs from bg (outer) to glow colour (inner) */
static void _soft_disc(int hdc, int cx, int cy, int r, int col, int steps)
{
    int k;
    for (k = 0; k <= steps; k++)
    {
        int rr = r - r * k / (steps + 1);
        _fill_disc(hdc, cx, cy, rr, _mix(C_BG_BOT, col, k + 1, steps + 1));
    }
}

/* wide wave: polyline drawn 3x with y offsets (fake blur),
 * colour gradient blue->violet across x */
static void _draw_wave(int hdc, int ybase, int amp, int phase)
{
    int off;
    for (off = -1; off <= 1; off++)
    {
        int x, px = 0, py = 0, first = 1;
        for (x = 0; x <= SCR_W; x += 3)
        {
            int y = ybase + off * 2 + amp * _isin((x * 360 / SCR_W) + phase) / 256;
            int c = _mix(C_WAVE_L, C_WAVE_R, x, SCR_W);
            if (off != 0) c = _mix(c, C_BG_BOT, 1, 2);
            gui_dc_set_color(hdc, c);
            if (!first) gui_dc_draw_line(hdc, px, py, x, y);
            px = x; py = y; first = 0;
        }
    }
}

static void _draw_text(int hdc, const char *s, int x0, int y0, int x1, int y1,
                       int align, int color)
{
    gui_rect_t r;
    r.x0 = (short)x0; r.y0 = (short)y0; r.x1 = (short)x1; r.y1 = (short)y1;
    gui_dc_set_color(hdc, color);
    gui_dc_display_string_in_rect(hdc, (char *)s, &r, align, GUI_MBCS_ENC);
}

/* like _draw_text, but truncates the string (with an ellipsis) to the rect width */
static void _draw_text_fit(int hdc, const char *s, int x0, int y0, int x1, int y1,
                           int align, int color)
{
    char buf[160];
    size_t n, orig;
    int wmax = x1 - x0;

    if (s == NULL) return;
    n = strlen(s);
    if (n >= sizeof(buf)) n = sizeof(buf) - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    orig = n;

    while (n > 1 && gui_dc_get_string_dist_x(hdc, buf, GUI_MBCS_ENC) > wmax)
    {
        n--;
        buf[n] = '\0';
    }
    if (n < orig && n >= 3)
    {
        buf[n - 1] = '.';
        buf[n - 2] = '.';
        buf[n - 3] = '.';
    }
    _draw_text(hdc, buf, x0, y0, x1, y1, align, color);
}

/* rounded-rectangle hit test */
static int _in_rrect(int px, int py, int x0, int y0, int x1, int y1, int r)
{
    int dx = 0, dy = 0;
    if (px < x0 || px > x1 || py < y0 || py > y1) return 0;
    if (r <= 0) return 1;
    if (px < x0 + r)      dx = (x0 + r) - px;
    else if (px > x1 - r) dx = px - (x1 - r);
    if (py < y0 + r)      dy = (y0 + r) - py;
    else if (py > y1 - r) dy = py - (y1 - r);
    if (dx == 0 || dy == 0) return 1;
    return dx * dx + dy * dy <= r * r;
}

/* selection highlight: 4px rounded outline in a theme-derived colour */
static void _sel_box(int hdc, int x0, int y0, int x1, int y1)
{
    const int t = 2, r = 3;
    int x, y, ir = r - t;

    if (x1 - x0 < 2 * t + 4 || y1 - y0 < 2 * t + 4) return;
    if (ir < 1) ir = 1;

    gui_dc_set_color(hdc, _mix(C_BAR_M, 0xFFFFFF, 120, 255));
    gui_dc_set_alpha(hdc, 230);
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++)
            if (_in_rrect(x, y, x0, y0, x1, y1, r) &&
                !_in_rrect(x, y, x0 + t, y0 + t, x1 - t, y1 - t, ir))
                gui_dc_draw_line(hdc, x, y, x, y);
    gui_dc_set_alpha(hdc, 255);
}

/* ---- box-art covers -------------------------------------------------------- */
#define COV_MAXW 256
#define COV_MAXH 256
static uint16_t s_cov[COV_MAXW * COV_MAXH];    /* decoded cover (RGB565)   */
static uint16_t s_covfit[COV_MAXW * COV_MAXH]; /* foreground (contain)     */
static uint16_t s_covbg[COV_MAXW * COV_MAXH];  /* background (blur mosaic) */

/* cover file: "C565" + u16 width + u16 height + w*h little-endian RGB565.
 * Name == the game (ROM file name, no extension). */
static int _cover_load(const char *name, int *pw, int *ph)
{
    static const char *dirs[] = {
        APPS "/game_launcher",          /* right next to the ROM */
        APPS "/game_launcher/covers",
        APPS "/covers",
        "/mnt/diska/covers"
    };
    char path[200];
    int i;

    for (i = 0; i < (int)(sizeof(dirs) / sizeof(dirs[0])); i++)
    {
        unsigned char hdr[8];
        FILE *f;
        int w, h;

        snprintf(path, sizeof(path), "%s/%s.565", dirs[i], name);
        f = fopen(path, "rb");
        if (f == NULL) continue;
        if (fread(hdr, 1, 8, f) != 8 ||
            hdr[0] != 'C' || hdr[1] != '5' || hdr[2] != '6' || hdr[3] != '5')
        { fclose(f); continue; }
        w = hdr[4] | (hdr[5] << 8);
        h = hdr[6] | (hdr[7] << 8);
        if (w < 1 || h < 1 || w > COV_MAXW || h > COV_MAXH) { fclose(f); continue; }
        if (fread(s_cov, 2, (size_t)w * h, f) != (size_t)w * h) { fclose(f); continue; }
        fclose(f);
        *pw = w; *ph = h;
        return 1;
    }
    return 0;
}

/* draw the cover (scaled to fit, centered) inside the frame; placeholder if none */
static void _cover_draw(int hdc, const char *name, int fx0, int fy0, int fx1, int fy1)
{
    int fw, fh, cw = 0, ch = 0, dw, dh, x, y;

    if (fx1 - fx0 < 10 || fy1 - fy0 < 10) return;

    fw = fx1 - fx0; fh = fy1 - fy0;
    if (fw > COV_MAXW || fh > COV_MAXH) return;

    if (_cover_load(name, &cw, &ch))
    {
        /* background: cheap "blur" = coarse mosaic of the whole cover, darkened,
         * so a non-matching aspect doesn't leave empty bars */
        int cols = 24, rows = 14, cx, cy;
        for (cy = 0; cy < rows; cy++)
        {
            int ry0 = cy * fh / rows, ry1 = (cy + 1) * fh / rows;
            int sy0 = cy * ch / rows, sy1 = (cy + 1) * ch / rows;
            if (ry1 <= ry0) ry1 = ry0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;
            for (cx = 0; cx < cols; cx++)
            {
                int rx0 = cx * fw / cols, rx1 = (cx + 1) * fw / cols;
                int sx0 = cx * cw / cols, sx1 = (cx + 1) * cw / cols;
                unsigned long rs = 0, gs = 0, bs = 0;
                unsigned v; int n = 0, xx, yy, rr, gg, bb;
                if (rx1 <= rx0) rx1 = rx0 + 1;
                if (sx1 <= sx0) sx1 = sx0 + 1;
                for (yy = sy0; yy < sy1 && yy < ch; yy++)
                    for (xx = sx0; xx < sx1 && xx < cw; xx++)
                    {
                        v = s_cov[(size_t)yy * cw + xx];
                        rs += (v >> 11) & 31; gs += (v >> 5) & 63; bs += v & 31; n++;
                    }
                rr = n ? (int)(rs / n) : 0;
                gg = n ? (int)(gs / n) : 0;
                bb = n ? (int)(bs / n) : 0;
                rr = rr * 5 / 8; gg = gg * 5 / 8; bb = bb * 5 / 8;   /* darken */
                v = ((unsigned)rr << 11) | ((unsigned)gg << 5) | (unsigned)bb;
                for (yy = ry0; yy < ry1; yy++)
                    for (xx = rx0; xx < rx1; xx++)
                        s_covbg[(size_t)yy * fw + xx] = (uint16_t)v;
            }
        }
        gui_dc_draw_bitmap_ext(hdc, (unsigned char *)s_covbg, fx0, fy0, fw, fh, 2);

        /* foreground: cover scaled to FIT (contain); if the aspect matches the
         * area it fills it 1:1, otherwise the blurred backdrop shows through */
        if ((long)cw * fh <= (long)ch * fw) { dw = fw; dh = (int)((long)ch * fw / cw); }
        else                                { dh = fh; dw = (int)((long)cw * fh / ch); }
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
        if (dw > COV_MAXW) dw = COV_MAXW;
        if (dh > COV_MAXH) dh = COV_MAXH;

        for (y = 0; y < dh; y++)
        {
            int sy = (int)((long)y * ch / dh);
            for (x = 0; x < dw; x++)
            {
                int sx = (int)((long)x * cw / dw);
                s_covfit[(size_t)y * dw + x] = s_cov[(size_t)sy * cw + sx];
            }
        }
        gui_dc_draw_bitmap_ext(hdc, (unsigned char *)s_covfit,
                               fx0 + (fw - dw) / 2, fy0 + (fh - dh) / 2, dw, dh, 2);
    }
    else
    {
        for (y = fy0; y < fy1; y++)
        {
            gui_dc_set_color(hdc, _mix(C_PANEL_T, C_PANEL_B, y - fy0, fy1 - fy0));
            gui_dc_draw_line(hdc, fx0, y, fx1, y);
        }
        _draw_text_fit(hdc, name, fx0, fy0, fx1, fy1,
                       GUI_ALIGN_CENTER | GUI_ALIGN_MIDDLE, C_HINT);
    }
}

/* ---- theme / config / system ---------------------------------------------- */
static void _apply_theme(int t)
{
    if (t < 0 || t >= NTHEMES) t = 0;
    s_theme  = t;
    C_BG_TOP = s_themes[t].top;
    C_BG_BOT = s_themes[t].bot;
    C_WAVE_L = s_themes[t].wl;
    C_WAVE_R = s_themes[t].wr;
    C_GLOW   = s_themes[t].glow;
    C_BAR_M  = s_themes[t].bar;
    /* panel = the background lightened a touch, so it tracks the theme */
    C_PANEL_T    = _mix(C_BG_TOP, 0xFFFFFF, 40, 255);
    C_PANEL_B    = _mix(C_BG_BOT, 0xFFFFFF, 40, 255);
    C_PANEL_EDGE = _mix(C_BG_BOT, 0xFFFFFF, 110, 255);
}

static const char *_theme_name(int i)
{
    switch (i)
    {
    case 0:  return T_(T_BLUE);
    case 1:  return T_(T_TEAL);
    case 2:  return T_(T_SUNSET);
    default: return T_(T_MONO);
    }
}

static void _load_cfg(void)
{
    FILE *f = fopen(CFG_PATH, "r");
    char line[64];

    if (f != NULL)
    {
        while (fgets(line, sizeof(line), f) != NULL)
        {
            if (strncmp(line, "theme=", 6) == 0) s_theme = line[6] - '0';
            else if (strncmp(line, "group=", 6) == 0) s_group_emu = (line[6] == '1');
            else if (strncmp(line, "covers=", 7) == 0) s_cov_on = (line[7] == '1');
            else if (strncmp(line, "lang=", 5) == 0) s_lang = line[5] - '0';
        }
        fclose(f);
    }
    _apply_theme(s_theme);
}

static void _save_cfg(void)
{
    FILE *f = fopen(CFG_PATH, "w");
    char buf[48];

    if (f == NULL) return;
    snprintf(buf, sizeof(buf), "theme=%d\ngroup=%d\ncovers=%d\nlang=%d\n",
             s_theme, s_group_emu ? 1 : 0, s_cov_on ? 1 : 0, s_lang);
    fputs(buf, f);
    fclose(f);
}

/* reboot via the hardware watchdog (as the SDK's software watchdog does) */
static void _draw_settings(int hdc);    /* fwd */
static void _draw_game_split(int hdc);  /* fwd */
static void _draw_flat_covers(int hdc); /* fwd */
static void _build_model(void);         /* fwd */
static int  _create_ui_font(int size);  /* fwd */

static void _system_reboot(void)
{
    int fd = open("/dev/wd", O_RDWR, 0);
    if (fd >= 0)
        ioctl(fd, WDD_CMD_SET_TIME, 1000);   /* resets in ~1 s */
    for (;;) { }                             /* wait for the reset */
}

static void _system_poweroff(void)
{
    msg_apps_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_POWER_OFF;
    broadcast_msg(&msg);
}

/* switch UI language: rebuild labels and reload a font with the right script */
static void _set_language(int lang)
{
    if (lang != s_lang)
    {
        s_lang = lang;
        _save_cfg();
        _build_model();
        s_cat = 0; s_item = 0; s_focus = 0; s_sub = 0; s_gsel = 0;
        if (s_font > 0) { gui_destroy_font(s_font); s_font = -1; }
        s_font = _create_ui_font(20);
        if (s_font > 0) gui_dc_set_default_fontface(s_font);
    }
    else
    {
        _save_cfg();
    }
}

/* ---- painting -------------------------------------------------------------- */
static void _draw(void)
{
    int hdc = s_hdc;
    int i, y;

    if (hdc <= 0) return;

    /* background gradient (#0A0E1A -> #1A1F3A) */
    for (y = 0; y < SCR_H; y++)
    {
        gui_dc_set_color(hdc, _mix(C_BG_TOP, C_BG_BOT, y, SCR_H));
        gui_dc_draw_line(hdc, 0, y, SCR_W - 1, y);
    }

    /* waves (static, wide, blue-violet) */
    _draw_wave(hdc, 150, 40, 0);
    _draw_wave(hdc, 150, 32, 60);
    _draw_wave(hdc, 150, 24, 120);

    /* horizontal category bar: active centred, bigger/brighter */
    {
        int slot = 170, cx = SCR_W / 2;
        for (i = 0; i < s_ncat; i++)
        {
            int x   = cx + (i - s_cat) * slot;
            int sel = (i == s_cat);
            const xmb_icon_t *ic = NULL;
            int half = 70;

            if (x + half < 0 || x - half > SCR_W) continue;
            if (s_cats[i].icon >= 0 && s_cats[i].icon < XMB_CAT_ICON_COUNT)
                ic = sel ? &xmb_cat_icon_hi[s_cats[i].icon]
                         : &xmb_cat_icon_lo[s_cats[i].icon];

            if (sel)
                _soft_disc(hdc, x, 60, 38, C_GLOW, 8);   /* very soft, dim halo */

            if (ic != NULL)
                gui_dc_draw_bitmap_ext(hdc, (unsigned char *)ic->px,
                                       x - ic->w / 2, 62 - ic->h / 2,
                                       ic->w, ic->h, 3);

            gui_dc_set_font_size(hdc, sel ? 20 : 16);
            _draw_text(hdc, s_cats[i].name, x - half, 104, x + half, 126,
                       GUI_ALIGN_CENTER | GUI_ALIGN_MIDDLE,
                       sel ? C_CAT_SEL : C_CAT_DIM);
        }
    }

    if (s_group_emu && s_cat == s_cat_game)
    {
        _draw_game_split(hdc);          /* grouped: emulators (left) + games (right) */
    }
    else if (s_cov_on && s_cat == s_cat_game)
    {
        _draw_flat_covers(hdc);         /* flat games + cover preview pane */
    }
    else
    {
    /* vertical item list: left aligned; highlight bar fades into the background
     * and runs off the right edge (dim, does not cover the text).
     * Only a window of "rows" items is shown, scrolled to keep the selection. */
    {
        int x0 = 56, x1 = x0 + 320;
        int y0 = 150, lh = 21;
        int n  = s_cats[s_cat].n;
        int rows = (SCR_H - 8 - y0) / lh;     /* visible rows */
        int top, last;

        if (rows < 1) rows = 1;
        top = s_item - rows / 2;
        if (top > n - rows) top = n - rows;
        if (top < 0) top = 0;
        last = top + rows;
        if (last > n) last = n;

        gui_dc_set_font_size(hdc, 14);
        for (i = top; i < last; i++)
        {
            int iy0 = y0 + (i - top) * lh;
            int iy1 = iy0 + lh - 3;
            if (i == s_item)
                _sel_box(hdc, 48, iy0 - 1, SCR_W - 48, iy1 + 2);
            _draw_text_fit(hdc, s_cats[s_cat].items[i], x0, iy0, x1, iy1,
                           GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE,
                           (i == s_item) ? C_ITEM_SEL : C_ITEM_DIM);
        }

        /* up/down chevrons when the list scrolls */
        if (top > 0)
            _draw_text(hdc, "..." , x0, y0 - 12, x0 + 40, y0 - 1,
                       GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_HINT);
        if (last < n)
            _draw_text(hdc, "...", x0, y0 + rows * lh + 1, x0 + 40, y0 + rows * lh + 12,
                       GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_HINT);
    }
    }

    /* hints, bottom-right, small */
    {
        const char *hint = T_(T_HINT_MAIN);
        int w;
        if (s_group_emu && s_cat == s_cat_game)
            hint = (s_focus == 1) ? T_(T_HINT_GAME) : T_(T_HINT_OPEN);
        gui_dc_set_font_size(hdc, 12);
        w = gui_dc_get_string_dist_x(hdc, (char *)hint, GUI_MBCS_ENC);
        _draw_text(hdc, hint, SCR_W - 12 - w, SCR_H - 18, SCR_W - 8, SCR_H - 4,
                   GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_HINT);
    }

    if (s_settings)
        _draw_settings(hdc);
}

/* ---- input ----------------------------------------------------------------- */
static void _move_cat(int d)
{
    s_cat += d;
    if (s_cat < 0) s_cat = s_ncat - 1;
    if (s_cat >= s_ncat) s_cat = 0;
    s_item = 0;
    s_focus = 0;
}

static void _move_item(int d)
{
    int n = s_cats[s_cat].n;
    if (n <= 0) { s_item = 0; return; }
    s_item += d;
    if (s_item < 0) s_item = n - 1;
    if (s_item >= n) s_item = 0;
}

/* Ask the process manager to start an app: content = "<app-path> [args]". */
static void _launch(const char *app, const char *arg)
{
    msg_apps_t msg;

    if (app == NULL) return;

    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_CREAT_APP;
    /* the manager parses this as a command line; quote the path so names with
     * spaces are kept as a single argv entry */
    if (arg != NULL && arg[0] != '\0')
        snprintf(msg.content, sizeof(msg.content), "\"%s\" %s", app, arg);
    else
        snprintf(msg.content, sizeof(msg.content), "\"%s\"", app);

    if (send_async_msg(APP_NAME_MANAGER, &msg) == false)
        print_err("launcher: failed to start %s\n", app);
}

static void _on_key(unsigned int kv)
{
    int k;
    if (IS_KEY_UP(kv)) return;      /* act on press only */
    k = (int)CLEAR_KEY_UP(kv);

    if (s_settings)
    {
        if (s_set_page == 0)                    /* main settings */
        {
            int n = 6;
            switch (k)
            {
            case KEY_VADD: if (--s_set_sel < 0) s_set_sel = n - 1; break;
            case KEY_VSUB: if (++s_set_sel >= n) s_set_sel = 0;    break;
            case KEY_OK:
                if (s_set_sel == 0)      { s_set_page = 1; s_set_sel = s_theme; }
                else if (s_set_sel == 1) { s_set_page = 2; s_set_sel = s_lang; }
                else if (s_set_sel == 2) { s_group_emu = !s_group_emu; _save_cfg();
                                           _build_model(); s_cat = 0; s_item = 0;
                                           s_focus = 0; s_sub = 0; s_gsel = 0; }
                else if (s_set_sel == 3) { s_cov_on = !s_cov_on; _save_cfg(); }
                else if (s_set_sel == 4) { _system_reboot(); }
                else                     { _system_poweroff(); s_exit = true; exit_msg_loop(); return; }
                break;
            case KEY_RETURN:
            case KEY_MENU: s_settings = false; s_set_sel = 0; break;
            default: return;
            }
        }
        else if (s_set_page == 1)               /* appearance */
        {
            int n = NTHEMES;
            switch (k)
            {
            case KEY_VADD: if (--s_set_sel < 0) s_set_sel = n - 1; _apply_theme(s_set_sel); break;
            case KEY_VSUB: if (++s_set_sel >= n) s_set_sel = 0;    _apply_theme(s_set_sel); break;
            case KEY_OK:   _apply_theme(s_set_sel); _save_cfg(); s_set_page = 0; s_set_sel = 0; break;
            case KEY_RETURN:
            case KEY_MENU: s_set_page = 0; s_set_sel = 0; break;
            default: return;
            }
        }
        else                                    /* language */
        {
            int n = 2;
            switch (k)
            {
            case KEY_VADD: if (--s_set_sel < 0) s_set_sel = n - 1; break;
            case KEY_VSUB: if (++s_set_sel >= n) s_set_sel = 0;    break;
            case KEY_OK:   _set_language(s_set_sel); s_set_page = 0; s_set_sel = 0; break;
            case KEY_RETURN:
            case KEY_MENU: s_set_page = 0; s_set_sel = 0; break;
            default: return;
            }
        }
        _draw();
        gui_screen_update();
        return;
    }

    /* grouped Game: emulator submenu (left) + games (right) */
    if (s_group_emu && s_cat == s_cat_game)
    {
        switch (k)
        {
        case KEY_PREV:  _move_cat(-1); s_focus = 0; break;
        case KEY_NEXT:  _move_cat(1);  s_focus = 0; break;
        case KEY_VADD:
            if (s_focus == 0) { if (--s_sub < 0) s_sub = 3; s_gsel = 0; }
            else { int n = s_gn[s_sub]; if (n > 0 && --s_gsel < 0) s_gsel = n - 1; }
            break;
        case KEY_VSUB:
            if (s_focus == 0) { if (++s_sub >= 4) s_sub = 0; s_gsel = 0; }
            else { int n = s_gn[s_sub]; if (n > 0 && ++s_gsel >= n) s_gsel = 0; }
            break;
        case KEY_OK:
            if (s_focus == 0)      { if (s_gn[s_sub] > 0) s_focus = 1; }
            else if (s_gn[s_sub])  { _launch(APPS "/emulator/emulator.app", s_garg[s_sub][s_gsel]); }
            break;
        case KEY_RETURN:
            if (s_focus == 1) { s_focus = 0; break; }
            s_exit = true; exit_msg_loop(); return;
        case KEY_MENU:  s_settings = true; s_set_page = 0; s_set_sel = 0; break;
        default: return;
        }
        _draw();
        gui_screen_update();
        return;
    }

    switch (k)
    {
    case KEY_PREV:  _move_cat(-1);  break;   /* left  */
    case KEY_NEXT:  _move_cat(1);   break;   /* right */
    case KEY_VADD:  _move_item(-1); break;   /* up    */
    case KEY_VSUB:  _move_item(1);  break;   /* down  */
    case KEY_OK:
        _launch(s_cats[s_cat].app[s_item], s_cats[s_cat].arg[s_item]);
        break;
    case KEY_MENU:  s_settings = true; s_set_page = 0; s_set_sel = 0; break;
    case KEY_RETURN: s_exit = true; exit_msg_loop(); return;
    default:        return;
    }

    _draw();
    gui_screen_update();
}

/* settings overlay, drawn in the same style on top of the XMB */
static void _draw_settings(int hdc)
{
    int px0 = 96, py0 = 40, px1 = SCR_W - 96, py1 = SCR_H - 40;
    int i, y, n, sel;
    const char *items[8];
    const char *title;

    for (y = py0; y <= py1; y++)
    {
        gui_dc_set_color(hdc, _mix(C_PANEL_T, C_PANEL_B, y - py0, py1 - py0));
        gui_dc_draw_line(hdc, px0, y, px1, y);
    }
    gui_dc_set_color(hdc, C_PANEL_EDGE);
    gui_dc_draw_line(hdc, px0, py0, px1, py0);
    gui_dc_draw_line(hdc, px0, py1, px1, py1);
    gui_dc_draw_line(hdc, px0, py0, px0, py1);
    gui_dc_draw_line(hdc, px1, py0, px1, py1);

    if (s_set_page == 0)
    {
        title = T_(T_SET_TITLE);
        n = 6;
        items[0] = T_(T_APPEARANCE);
        items[1] = T_(T_LANGUAGE);
        items[2] = s_group_emu ? T_(T_GROUP_ON) : T_(T_GROUP_OFF);
        items[3] = s_cov_on ? T_(T_COVERS_ON) : T_(T_COVERS_OFF);
        items[4] = T_(T_REBOOT);
        items[5] = T_(T_POWEROFF);
    }
    else if (s_set_page == 1)
    {
        title = T_(T_APPEAR_TITLE);
        n = NTHEMES;
        for (i = 0; i < NTHEMES; i++) items[i] = _theme_name(i);
    }
    else
    {
        title = T_(T_LANG_TITLE);
        n = 2;
        items[0] = T_(T_RUSSIAN);
        items[1] = T_(T_ENGLISH);
    }

    gui_dc_set_font_size(hdc, 16);
    _draw_text(hdc, title, px0 + 14, py0 + 6, px1 - 14, py0 + 28,
               GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_CAT_SEL);

    sel = s_set_sel;
    gui_dc_set_font_size(hdc, 14);
    for (i = 0; i < n; i++)
    {
        int iy = py0 + 36 + i * 21;
        if (i == sel)
        {
            int bx;
            for (y = iy; y < iy + 18; y++)
            {
                int bg = _mix(C_PANEL_T, C_PANEL_B, y - py0, py1 - py0);
                gui_dc_set_color(hdc, _mix(bg, C_BAR_M, 150, 255));
                gui_dc_draw_line(hdc, px0 + 8, y, px1 - 8, y);
            }
        }
        _draw_text(hdc, items[i], px0 + 16, iy, px1 - 16, iy + 18,
                   GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE,
                   (i == sel) ? C_ITEM_SEL : C_ITEM_DIM);
    }

    gui_dc_set_font_size(hdc, 12);
    _draw_text(hdc, T_(T_HINT_SET), px0 + 16, py1 - 22, px1 - 16, py1 - 6,
               GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_HINT);
}

/* grouped "Game": vertical emulator submenu on the left, its games on the right */
static void _draw_game_split(int hdc)
{
    int y0 = 150, lh = 21, i;
    int lx0, lx1, rx0, rx1, div1, div2 = 0, cov_x = 0;
    int rows = (SCR_H - 8 - y0) / lh;
    int n, top, last, dy1;

    if (rows < 1) rows = 1;

    if (s_cov_on)                 /* categories | games | small square cover */
    {
        lx0 = 16;  lx1 = 96;   div1 = 112;
        rx0 = 122; rx1 = 332;  div2 = 348;
        cov_x = 358;
    }
    else                          /* emulators | games */
    {
        lx0 = 56;  lx1 = 168;
        rx0 = 200; rx1 = SCR_W - 12;
        div1 = (lx1 + rx0) / 2;
    }

    dy1 = y0 + rows * lh - 2;
    if (dy1 > SCR_H - 20) dy1 = SCR_H - 20;

    gui_dc_set_font_size(hdc, 14);

    /* left: emulators */
    for (i = 0; i < 4; i++)
    {
        int iy = y0 + i * lh;
        int on = (i == s_sub);
        if (on)
            _sel_box(hdc, 8, iy - 1, div1 - 8, iy + lh - 1);
        _draw_text_fit(hdc, s_emu_name[i], lx0, iy, lx1, iy + lh - 3,
                       GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE,
                       on ? C_ITEM_SEL : C_ITEM_DIM);
    }

    /* dividers: short (centred) and 50% transparent */
    {
        int s = dy1 - 6 - y0;
        int l = s * 7 / 30;                 /* ~1/3 minus 30% */
        int dtop = y0 + (s - l) / 2;
        int dbot = dtop + l;
        gui_dc_set_color(hdc, C_PANEL_EDGE);
        gui_dc_set_alpha(hdc, 128);
        gui_dc_draw_line(hdc, div1, dtop, div1, dbot);
        if (s_cov_on)
            gui_dc_draw_line(hdc, div2, dtop, div2, dbot);
        gui_dc_set_alpha(hdc, 255);
    }

    /* middle: games of the selected emulator */
    n = s_gn[s_sub];
    top = s_gsel - rows / 2;
    if (top > n - rows) top = n - rows;
    if (top < 0) top = 0;
    last = top + rows;
    if (last > n) last = n;

    if (n == 0)
        _draw_text(hdc, T_(T_NOGAMES), rx0, y0, rx1, y0 + lh,
                   GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_HINT);

    for (i = top; i < last; i++)
    {
        int iy = y0 + (i - top) * lh;
        int bright = (s_focus == 1) && (i == s_gsel);
        int gx1 = s_cov_on ? div2 : SCR_W;
        int gx0 = rx0 - 8;                 /* 8px padding from the text ... */
        if (gx0 < div1 + 3) gx0 = div1 + 3;/* ... but keep >=3px off the divider */
        if (bright)
            _sel_box(hdc, gx0, iy - 1, gx1 - 8, iy + lh - 1);
        _draw_text_fit(hdc, s_glab[s_sub][i], rx0, iy, rx1, iy + lh - 3,
                       GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE,
                       bright ? C_ITEM_SEL : C_ITEM_DIM);
    }

    if (top > 0)
        _draw_text(hdc, "...", rx0, y0 - 12, rx0 + 40, y0 - 1,
                   GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_HINT);
    if (last < n)
        _draw_text(hdc, "...", rx0, y0 + rows * lh + 1, rx0 + 40, y0 + rows * lh + 12,
                   GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_HINT);

    /* right: small square cover preview of the selected game */
    if (s_cov_on && n > 0)
    {
        int sc = dy1 - (y0 - 6);            /* square side */
        int availw = (SCR_W - 8) - cov_x;
        int cx;
        if (sc > availw) sc = availw;
        cx = cov_x + (availw - sc) / 2;
        _cover_draw(hdc, s_glab[s_sub][s_gsel], cx, y0 - 6, cx + sc, y0 - 6 + sc);
    }
}

/* flat "Game" list with a divider and a (larger) cover preview in the right half */
static void _draw_flat_covers(int hdc)
{
    int y0 = 150, lh = 21, i;
    int x0 = 56, x1 = 222, div = 240;
    int cov0 = 250, cov1 = SCR_W - 48;
    int n = s_cats[s_cat].n;
    int rows = (SCR_H - 8 - y0) / lh;
    int top, last, dy1;

    if (rows < 1) rows = 1;
    top = s_item - rows / 2;
    if (top > n - rows) top = n - rows;
    if (top < 0) top = 0;
    last = top + rows;
    if (last > n) last = n;
    dy1 = y0 + rows * lh - 2;
    if (dy1 > SCR_H - 20) dy1 = SCR_H - 20;

    {
        int s = dy1 - 6 - y0;
        int l = s * 7 / 30;                 /* ~1/3 minus 30% */
        int dtop = y0 + (s - l) / 2;
        gui_dc_set_color(hdc, C_PANEL_EDGE);
        gui_dc_set_alpha(hdc, 128);
        gui_dc_draw_line(hdc, div, dtop, div, dtop + l);
        gui_dc_set_alpha(hdc, 255);
    }

    gui_dc_set_font_size(hdc, 14);
    for (i = top; i < last; i++)
    {
        int iy = y0 + (i - top) * lh;
        if (i == s_item)
            _sel_box(hdc, 48, iy - 1, div - 8, iy + lh - 1);
        _draw_text_fit(hdc, s_cats[s_cat].items[i], x0, iy, x1, iy + lh - 3,
                       GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE,
                       (i == s_item) ? C_ITEM_SEL : C_ITEM_DIM);
    }
    if (top > 0)
        _draw_text(hdc, "...", x0, y0 - 12, x0 + 40, y0 - 1,
                   GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_HINT);
    if (last < n)
        _draw_text(hdc, "...", x0, y0 + rows * lh + 1, x0 + 40, y0 + rows * lh + 12,
                   GUI_ALIGN_LEFT | GUI_ALIGN_MIDDLE, C_HINT);

    if (n > 0)
        _cover_draw(hdc, s_cats[s_cat].items[s_item], cov0, y0 - 6, cov1, dy1);
}

static void _win_cb(gui_wm_message_t *pmsg)
{
    switch (pmsg->msgid)
    {
    case WM_MSG_PAINT:
        _draw();
        gui_screen_update();
        break;
    case WM_MSG_KEY:
        _on_key(pmsg->data.v);
        break;
    default:
        break;
    }
}

int proc_sys_msg(msg_apps_t *msg)
{
    switch (msg->type)
    {
    case MSG_APP_QUIT:
        s_exit = true;
        exit_msg_loop();
        break;
    default:
        break;
    }
    return 1;
}

/* ---- model ----------------------------------------------------------------- */
/* owners of the dynamic label/path/arg strings */
static char s_itembuf[MAX_CATS][MAX_ITEMS][64];
static char s_appbuf [MAX_CATS][MAX_ITEMS][160];
static char s_argbuf [MAX_CATS][MAX_ITEMS][80];

static int _new_cat(const char *name, int icon)
{
    int i = s_ncat;
    if (i >= MAX_CATS) return -1;
    s_cats[i].name = name;
    s_cats[i].icon = icon;
    s_cats[i].n    = 0;
    s_ncat++;
    return i;
}

static int _add_item(int cat, const char *label, const char *app, const char *arg)
{
    int j = s_cats[cat].n;
    if (j >= MAX_ITEMS) return j;
    if (label != NULL)
    {
        strncpy(s_itembuf[cat][j], label, 63); s_itembuf[cat][j][63] = 0;
        s_cats[cat].items[j] = s_itembuf[cat][j];
    }
    if (app != NULL)
    {
        strncpy(s_appbuf[cat][j], app, 159); s_appbuf[cat][j][159] = 0;
        s_cats[cat].app[j] = s_appbuf[cat][j];
    }
    if (arg != NULL)
    {
        strncpy(s_argbuf[cat][j], arg, 79); s_argbuf[cat][j][79] = 0;
        s_cats[cat].arg[j] = s_argbuf[cat][j];
    }
    s_cats[cat].n = j + 1;
    return j;
}

static int _ends_with(const char *s, const char *ext)
{
    size_t ls = strlen(s), le = strlen(ext);
    return ls >= le && strcasecmp(s + ls - le, ext) == 0;
}

static const char *_game_mode(const char *name)
{
    if (_ends_with(name, ".nes")) return "NES";
    if (_ends_with(name, ".gba") || _ends_with(name, ".gbc") || _ends_with(name, ".gb")) return "GBC";
    if (_ends_with(name, ".smc") || _ends_with(name, ".sfc")) return "SFC";
    if (_ends_with(name, ".smd") || _ends_with(name, ".md") || _ends_with(name, ".bin")) return "MD";
    return NULL;
}

/* friendly display names keyed by the app directory name */
typedef struct { const char *dir; const char *en; const char *ru; } namemap_t;
static const namemap_t s_namemap[] = {
    { "calculat",      "Calculator", "Калькулятор" }, { "calculator", "Calculator", "Калькулятор" },
    { "calendar",      "Calendar",   "Календарь"   }, { "stopwatch",  "Stopwatch",  "Секундомер" },
    { "watch",         "Stopwatch",  "Секундомер"  }, { "ebook",      "eBook",      "Читалка" },
    { "emulator",      "Games",      "Игры"        }, { "game_launcher", "Games",   "Игры" },
    { "music",         "Music",      "Музыка"      }, { "photo",      "Photos",     "Фото" },
    { "recorder",      "Recorder",   "Диктофон"    }, { "setting",    "Settings",   "Настройки" },
    { "video",         "Videos",     "Видео"       }, { "browser",    "Browser",    "Браузер" },
    { "radio",         "Radio",      "Радио"       }, { "camera",     "Camera",     "Камера" },
    { "tvin",          "TV In",      "ТВ-вход"     }, { "usb",        "USB",        "USB" },
};

/* dir/file stem -> a human-readable label (map lookup, else title-case) */
static void _pretty(const char *raw, char *out, size_t n)
{
    size_t i, j;
    int cap = 1;

    for (i = 0; i < sizeof(s_namemap) / sizeof(s_namemap[0]); i++)
    {
        if (strcasecmp(raw, s_namemap[i].dir) == 0)
        {
            strncpy(out, (s_lang == 0 ? s_namemap[i].ru : s_namemap[i].en), n - 1);
            out[n - 1] = '\0';
            return;
        }
    }
    for (j = 0; raw[j] != '\0' && j < n - 1; j++)
    {
        char c = raw[j];
        if (c == '_' || c == '-')
        {
            out[j] = ' ';
            cap = 1;
        }
        else if (cap)
        {
            out[j] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
            cap = 0;
        }
        else
        {
            out[j] = c;
        }
    }
    out[j] = '\0';
}

/* scan /mnt/diska/apps/<dir>/<dir>.app -> one entry per installed app */
static void _scan_apps(int cat)
{
    DIR *d = opendir(APPS);
    struct dirent *e;

    if (d == NULL) return;
    while ((e = readdir(d)) != NULL)
    {
        char sub[160], app[176], disp[64];
        struct stat st;

        if (e->d_name[0] == '.') continue;
        snprintf(sub, sizeof(sub), "%s/%s", APPS, e->d_name);
        if (stat(sub, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(app, sizeof(app), "%s/%s.app", sub, e->d_name);
        if (stat(app, &st) != 0) continue;
        _pretty(e->d_name, disp, sizeof(disp));
        _add_item(cat, disp, app, NULL);
    }
    closedir(d);
}

/* scan /mnt/diska/apps/game_launcher -> flat list in "cat" */
static void _scan_games(int cat)
{
    DIR *d = opendir(APPS "/game_launcher");
    struct dirent *e;

    if (d == NULL) return;
    while ((e = readdir(d)) != NULL)
    {
        const char *mode;
        char label[64], path[160], arg[96], *dot;

        if (e->d_name[0] == '.') continue;
        mode = _game_mode(e->d_name);
        if (mode == NULL) continue;
        snprintf(label, sizeof(label), "%s", e->d_name);
        if ((dot = strrchr(label, '.')) != NULL) *dot = '\0';
        snprintf(path, sizeof(path), "%s/game_launcher/%s", APPS, e->d_name);
        snprintf(arg, sizeof(arg), "%s \"%s\"", mode, path);
        _add_item(cat, label, APPS "/emulator/emulator.app", arg);
    }
    closedir(d);
}

/* scan game_launcher, split by emulator (for the vertical grouped view) */
static void _scan_games_grouped(void)
{
    DIR *d = opendir(APPS "/game_launcher");
    struct dirent *e;
    int i;

    for (i = 0; i < 4; i++) s_gn[i] = 0;
    if (d == NULL) return;
    while ((e = readdir(d)) != NULL)
    {
        const char *mode;
        char label[64], path[160], arg[96], *dot;
        int m;

        if (e->d_name[0] == '.') continue;
        mode = _game_mode(e->d_name);
        if (mode == NULL) continue;
        for (m = 0; m < 4; m++)
            if (strcmp(mode, s_emu_mode[m]) == 0) break;
        if (m == 4 || s_gn[m] >= MAX_GITEMS) continue;

        snprintf(label, sizeof(label), "%s", e->d_name);
        if ((dot = strrchr(label, '.')) != NULL) *dot = '\0';
        snprintf(path, sizeof(path), "%s/game_launcher/%s", APPS, e->d_name);
        snprintf(arg, sizeof(arg), "%s \"%s\"", mode, path);
        snprintf(s_glab[m][s_gn[m]], sizeof(s_glab[0][0]), "%s", label);
        snprintf(s_garg[m][s_gn[m]], sizeof(s_garg[0][0]), "%s", arg);
        s_gn[m]++;
    }
    closedir(d);
}

static void _build_model(void)
{
    int i, cg;

    for (i = 0; i < MAX_CATS; i++) { s_cats[i].n = 0; s_cats[i].icon = -1; s_cats[i].name = ""; }
    for (i = 0; i < 4; i++) s_gn[i] = 0;
    s_ncat = 0;

    _new_cat(T_(T_PHOTO), 0);
    _new_cat(T_(T_MUSIC), 1);
    _new_cat(T_(T_VIDEO), 2);
    s_cat_game = _new_cat(T_(T_GAME), 3);
    cg         = _new_cat(T_(T_APPS), 4);
    _new_cat(T_(T_SETTING), 5);

    _add_item(0, T_(T_ALLPHOTOS), APPS "/photo/photo.app",     NULL);
    _add_item(0, T_(T_CAMERA),    APPS "/camera/camera.app",   NULL);
    _add_item(1, T_(T_MUSIC1),    APPS "/music/music.app",     NULL);
    _add_item(2, T_(T_VIDEOS),    APPS "/video/video.app",     NULL);
    _add_item(5, T_(T_SETTINGS),  APPS "/setting/setting.app", NULL);

    if (s_group_emu)
    {
        _scan_games_grouped();          /* vertical emulator submenu + games pane */
    }
    else
    {
        _scan_games(s_cat_game);
        if (s_cats[s_cat_game].n == 0)
            for (i = 0; i < 4; i++)
                _add_item(s_cat_game, s_emu_name[i], APPS "/emulator/emulator.app", s_emu_mode[i]);
    }

    _scan_apps(cg);
    if (s_cats[cg].n == 0)
        _add_item(cg, T_(T_GAMES), APPS "/emulator/emulator.app", NULL);

    s_sub = 0; s_gsel = 0;
}

/* ---- font ------------------------------------------------------------------ */
/* Load our bundled geometric font (xmb.ttf, shipped next to the .app);
 * fall back to the system default font. */
static int _try_font(const char *dir, const char *file, int size)
{
    char path[256];
    if (dir == NULL || dir[0] == '\0') return -1;
    sprintf(path, "%s/%s", dir, file);
    return gui_create_font(path, size);
}

/* one UI font for both languages (Latin + Cyrillic): ui.ttf */
static int _create_ui_font(int size)
{
    const char *file = "ui.ttf";
    int d = _try_font(sys_get_app_theme_res_dir(), file, size);
    if (d > 0) return d;
    d = _try_font("/mnt/diska/apps/launcher", file, size);
    if (d > 0) return d;
    d = _try_font("/mnt/diska/apps/launcher", "xmb.ttf", size);   /* fallback */
    if (d > 0) return d;

    return gui_create_font((char *)sys_get_default_font_file(), size);
}

/* ---- scene ----------------------------------------------------------------- */
int launcher_scene(fusion_enter_e enter_type)
{
    msg_apps_t msg;
    (void)enter_type;

    _load_cfg();
    _build_model();
    s_cat = 0; s_item = 0; s_exit = false;
    s_settings = false; s_set_page = 0; s_set_sel = 0;
    s_focus = 0; s_sub = 0; s_gsel = 0;

    s_font = _create_ui_font(20);

    s_win = gui_wm_create_window(0, 0, SCR_W, SCR_H, WM_CREATE_SHOW, _win_cb, 0);
    if (s_win < 0)
    {
        print_err("gui_wm_create_window failed\n");
        if (s_font > 0) gui_destroy_font(s_font);
        return -1;
    }

    s_hdc = gui_dc_get(s_win);
    if (s_hdc <= 0)
    {
        print_err("gui_dc_get failed\n");
        gui_wm_delete_window(s_win);
        if (s_font > 0) gui_destroy_font(s_font);
        return -1;
    }

    gui_wm_set_focus(s_win);
    if (s_font > 0) gui_dc_set_default_fontface(s_font);
    gui_dc_set_text_mode(s_hdc, GUI_DRAW_MODE_TRANS);

    _draw();
    gui_screen_update();

    register_sys_dispatcher(proc_sys_msg);
    while (get_msg(&msg))
        dispatch_msg(&msg);
    unregister_sys_dispatcher();

    if (s_win > 0) gui_wm_delete_window(s_win);
    if (s_font > 0) gui_destroy_font(s_font);
    s_win = -1; s_hdc = -1; s_font = -1;
    return 0;
}

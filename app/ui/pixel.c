/* pixel: the menu's look for apps that draw into a framebuffer. See pixel.h.
   Palettes, accents and shape recipes ported from JorreFog/BananaLinux (MIT, Copyright (c) 2026 JorreFog and the
   Banana contributors): ui/src/settings_pal.cpp, ui/rnds/RndsUI.cpp, store/apps/activity (DESIGN.md and its box
   painter). */
#include "pixel.h"
#include "stb_truetype.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

PxPal px;

/* ---- settings ---------------------------------------------------------------------------------------------- */

static void trim(char *s)
{
    char *a = s, *e;
    while (*a == ' ' || *a == '\t') a++;
    if (a != s) memmove(s, a, strlen(a) + 1);
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
}

static void put(char *dst, size_t n, const char *src)
{
    size_t i = 0;
    while (i + 1 < n && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

void px_settings_read(const char *path, PxSettings *st)
{
    char line[512];
    FILE *f;
    memset(st, 0, sizeof *st);
    put(st->theme, sizeof st->theme, "dark");
    put(st->accent, sizeof st->accent, "yellow");
    put(st->accent_text, sizeof st->accent_text, "auto");
    put(st->language, sizeof st->language, "en");
    put(st->clock, sizeof st->clock, "24h");
    if (!path) path = getenv("PIXEL_UI_SETTINGS");
    if (!path || !path[0]) path = "/data/banana/settings.conf";
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *eq, *k = line, *v;
        trim(line);
        if (!line[0] || line[0] == '#') continue;
        eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        v = eq + 1;
        trim(k);
        trim(v);
        if (!strcmp(k, "theme")) put(st->theme, sizeof st->theme, v);
        else if (!strcmp(k, "accent")) put(st->accent, sizeof st->accent, v);
        else if (!strcmp(k, "accent_text")) put(st->accent_text, sizeof st->accent_text, v);
        else if (!strcmp(k, "language")) put(st->language, sizeof st->language, v);
        else if (!strcmp(k, "clock")) put(st->clock, sizeof st->clock, v);
    }
    fclose(f);
}

/* ---- colours ----------------------------------------------------------------------------------------------- */

uint32_t px_mix(uint32_t a, uint32_t b, float t)
{
    int i;
    uint32_t out = 0;
    for (i = 0; i < 3; i++) {
        int sh = i * 8;
        float v = ((a >> sh) & 255) * (1 - t) + ((b >> sh) & 255) * t;
        out |= (uint32_t)(int)(v + 0.5f) << sh;
    }
    return out;
}

static float lum(uint32_t c)
{
    float ch[3];
    int i;
    for (i = 0; i < 3; i++) {
        float x = ((c >> (16 - i * 8)) & 255) / 255.0f;
        ch[i] = x <= 0.03928f ? x / 12.92f : powf((x + 0.055f) / 1.055f, 2.4f);
    }
    return 0.2126f * ch[0] + 0.7152f * ch[1] + 0.0722f * ch[2];
}

static float contrast(uint32_t a, uint32_t b)
{
    float la = lum(a), lb = lum(b);
    return la > lb ? (la + 0.05f) / (lb + 0.05f) : (lb + 0.05f) / (la + 0.05f);
}

static int g_accent_text;    /* 0 auto, 1 dark, 2 light (settings.conf's accent_text) */

/* RndsUI.cpp's inkOn: the text on an accent plate, dark or light by contrast */
uint32_t px_ink_on(uint32_t rgb)
{
    if (g_accent_text == 1) return 0x17161c;
    if (g_accent_text == 2) return 0xffffff;
    return contrast(rgb, 0x17161c) >= contrast(rgb, 0xffffff) ? 0x17161c : 0xffffff;
}

/* RndsUI.cpp's subInkOn: a second line on the plate, in the accent's own hue until it reads */
static uint32_t sub_ink_on(uint32_t rgb)
{
    uint32_t ink = px_ink_on(rgb), out = ink;
    float t;
    for (t = 0.5f; t <= 1.0f; t += 0.05f)
        if (contrast(out = px_mix(rgb, ink, t), rgb) >= 4.5f) return out;
    return ink;
}

static const struct { const char *name; PxAccent a; } ACCENTS[] = {
    { "yellow", { 0xf0b429, 0xffe68a, 0x8a5a00, 0xffcf5c, 0xc98f12 } },
    { "blue",   { 0x4a9fe0, 0xb7e1ff, 0x0f3f66, 0x7cc0f2, 0x2f7fc0 } },
    { "red",    { 0xe8574d, 0xffb8b0, 0x7a1a14, 0xf58a80, 0xc23b32 } },
    { "green",  { 0x56bf6b, 0xbff0c8, 0x1d5a2b, 0x86d896, 0x3d9a52 } },
    { "purple", { 0xa07ae6, 0xddd0ff, 0x43266e, 0xbfa4f2, 0x7f58c4 } },
    { "pink",   { 0xec6fa8, 0xffc6e0, 0x7a1f4a, 0xf59cc4, 0xc9508a } },
};

void px_pal_make(PxPal *p, const PxSettings *st)
{
    size_t i;
    PxAccent a = ACCENTS[0].a;
    for (i = 0; i < sizeof ACCENTS / sizeof ACCENTS[0]; i++)
        if (st && !strcmp(st->accent, ACCENTS[i].name)) a = ACCENTS[i].a;
    g_accent_text = st && !strcmp(st->accent_text, "dark") ? 1 : st && !strcmp(st->accent_text, "light") ? 2 : 0;
    memset(p, 0, sizeof *p);
    p->accent = a;
    p->light = st && !strcmp(st->theme, "light");
    if (!p->light) {
        p->ink = 0xf1ede2; p->muted = 0xa39d8e; p->line = 0x8a8374; p->edge = 0xe8e2d3; p->dim = 0x6b6559;
        p->screen_bg = 0x17161c; p->screen_frame = 0x0b0a0d; p->screen_band = 0x2e2b33;
        p->panel_band = 0x3a3540; p->panel_body = 0x17161c; p->panel_low = 0x0b0a0d;
        p->bubble_band = 0x38333b; p->bubble_body = 0x191820; p->bubble_low = 0x0f0e13;
        p->chip_band = 0x2e2b33; p->chip_body = 0x121117; p->rail_bg = 0x100f14; p->rule = 0x45403d;
        p->brand_bg = 0x262219; p->kbd_bg = 0xf1ede2; p->kbd_ink = 0x17161c;
        p->red = 0xff3b4a; p->green = 0x56bf6b; p->warn = 0xff8a96;
        p->dither_rgb = 0xffffff; p->dither_a = 36; p->dither_b = 18;
        p->sheet = px_mix(0x15141a, a.base, 0.34f);
        p->pick_edge = 0xffffff;
    } else {
        p->ink = 0x2a2620; p->muted = 0x7a7262; p->line = 0xa69d8c; p->edge = 0x3a342c; p->dim = 0xa69d8c;
        p->screen_bg = 0xf3efe6; p->screen_frame = 0xd4ccbd; p->screen_band = 0xfdfcf9;
        p->panel_band = 0xffffff; p->panel_body = 0xf6f2ea; p->panel_low = 0xd8d0c0;
        p->bubble_band = 0xffffff; p->bubble_body = 0xfaf7f0; p->bubble_low = 0xddd5c6;
        p->chip_band = 0xffffff; p->chip_body = 0xefe9dd; p->rail_bg = 0xddd5c6; p->rule = 0xc9c0ae;
        p->brand_bg = 0xfdfcf9; p->kbd_bg = 0x3a342c; p->kbd_ink = 0xf2f6fa;
        p->red = 0xe23b4a; p->green = 0x2f8f45; p->warn = 0xb8232f;
        p->dither_rgb = 0x3a2e14; p->dither_a = 18; p->dither_b = 9;
        p->sheet = px_mix(0xfdfcf9, a.base, 0.22f);
        p->pick_edge = a.dark;
    }
    p->pick = a.base; p->pick_band = a.band; p->pick_low = a.low;
    p->on_pick = px_ink_on(a.base); p->on_pick_sub = sub_ink_on(a.base);
}

/* ---- translations ------------------------------------------------------------------------------------------ */

static const PxTr *g_tr;
static char g_lang[16] = "en";

void px_lang_set(const char *code, const PxTr *table)
{
    put(g_lang, sizeof g_lang, code && code[0] ? code : "en");
    g_tr = table;
}

int px_lang_is(const char *code)
{
    size_t n = strlen(code);
    return !strncmp(g_lang, code, n) && (g_lang[n] == 0 || g_lang[n] == '-' || g_lang[n] == '_');
}

const char *px_tr(const char *en)
{
    const PxTr *t;
    if (!g_tr || !px_lang_is("es")) return en;
    for (t = g_tr; t->en; t++)
        if (!strcmp(t->en, en)) return t->es ? t->es : en;
    return en;
}

/* ---- fonts: glyphs rendered as they are asked for, per face and pixel size --------------------------------- */

#define PX_CPS 384                /* Basic Latin, Latin-1 and Latin Extended-A */
#define PX_SLOTS 24

typedef struct {
    unsigned char *bm;
    int w, h, xo, yo, adv, done;
} Glyph;

typedef struct {
    int face, ipx, used;
    float scale;
    int ascent;
    Glyph g[PX_CPS];
} Slot;

static unsigned char *g_ttf[2];
static stbtt_fontinfo g_font[2];
static int g_font_ok[2];
static Slot g_slot[PX_SLOTS];

static unsigned char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    unsigned char *b;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 1 || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    b = malloc((size_t)n);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    return b;
}

int px_fonts_load(const char *dir)
{
    static const char *const names[2] = { "PixelifySans-Regular.ttf", "PixelifySans-Medium.ttf" };
    char path[512];
    int i;
    if (!dir) dir = getenv("PIXEL_UI_FONTS");
    if (!dir || !dir[0]) dir = "/usr/share/banana/rnds/fonts";
    for (i = 0; i < 2; i++) {
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        g_ttf[i] = read_file(path);
        g_font_ok[i] = g_ttf[i] && stbtt_InitFont(&g_font[i], g_ttf[i], stbtt_GetFontOffsetForIndex(g_ttf[i], 0));
    }
    /* one face stands in for the other */
    if (!g_font_ok[1] && g_font_ok[0]) { g_font[1] = g_font[0]; g_font_ok[1] = 1; }
    if (!g_font_ok[0] && g_font_ok[1]) { g_font[0] = g_font[1]; g_font_ok[0] = 1; }
    return g_font_ok[0];
}

void px_fonts_free(void)
{
    int s, i;
    for (s = 0; s < PX_SLOTS; s++)
        for (i = 0; i < PX_CPS; i++) free(g_slot[s].g[i].bm);
    memset(g_slot, 0, sizeof g_slot);
    free(g_ttf[0]);
    free(g_ttf[1]);
    g_ttf[0] = g_ttf[1] = NULL;
    g_font_ok[0] = g_font_ok[1] = 0;
}

static Slot *slot(int face, int ipx)
{
    int s, asc, desc, gap;
    for (s = 0; s < PX_SLOTS; s++)
        if (g_slot[s].used && g_slot[s].face == face && g_slot[s].ipx == ipx) return &g_slot[s];
    for (s = 0; s < PX_SLOTS && g_slot[s].used; s++) {}
    if (s == PX_SLOTS) {                         /* full: start again (never in these apps) */
        int i, k;
        for (k = 0; k < PX_SLOTS; k++)
            for (i = 0; i < PX_CPS; i++) free(g_slot[k].g[i].bm);
        memset(g_slot, 0, sizeof g_slot);
        s = 0;
    }
    g_slot[s].used = 1;
    g_slot[s].face = face;
    g_slot[s].ipx = ipx;
    g_slot[s].scale = stbtt_ScaleForPixelHeight(&g_font[face], (float)ipx);
    stbtt_GetFontVMetrics(&g_font[face], &asc, &desc, &gap);
    g_slot[s].ascent = (int)(asc * g_slot[s].scale + 0.5f);
    return &g_slot[s];
}

static Glyph *glyph(Slot *sl, int cp)
{
    Glyph *g;
    int adv, lsb;
    if (cp < 0 || cp >= PX_CPS) cp = '?';
    g = &sl->g[cp];
    if (g->done) return g;
    g->done = 1;
    if (!stbtt_FindGlyphIndex(&g_font[sl->face], cp) && cp != ' ') {
        if (cp != '?') { *g = *glyph(sl, '?'); g->bm = NULL; g->done = 1; g->w = 0; return g; }
    }
    stbtt_GetCodepointHMetrics(&g_font[sl->face], cp, &adv, &lsb);
    g->adv = (int)(adv * sl->scale + 0.5f);
    g->bm = stbtt_GetCodepointBitmap(&g_font[sl->face], 0, sl->scale, cp, &g->w, &g->h, &g->xo, &g->yo);
    return g;
}

static int utf8(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    int cp;
    if (p[0] < 0x80) { *s += 1; return p[0]; }
    if ((p[0] & 0xe0) == 0xc0 && p[1]) { cp = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f); *s += 2; return cp; }
    if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2]) {
        cp = ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f); *s += 3; return cp;
    }
    *s += 1;
    return '?';
}

static int upper(int cp)
{
    if (cp >= 'a' && cp <= 'z') return cp - 32;
    if (cp >= 0xe0 && cp <= 0xfe && cp != 0xf7) return cp - 32;
    return cp;
}

int px_u(const PxCanvas *c, float units)
{
    return (int)lroundf(units * c->s);
}

static int text_px(const PxCanvas *c, int face, float size, const char *s, int flags, int *count)
{
    Slot *sl;
    int ipx = px_u(c, size), w = 0, n = 0, track = (flags & PX_TRACK) ? px_u(c, size * 0.11f) : 0;
    if (!g_font_ok[face] || ipx < 4) { if (count) *count = 0; return 0; }
    sl = slot(face, ipx);
    while (*s) {
        int cp = utf8(&s);
        if (flags & PX_UPPER) cp = upper(cp);
        w += glyph(sl, cp)->adv + (n ? track : 0);
        n++;
    }
    if (count) *count = n;
    return w;
}

float px_text_w(const PxCanvas *c, int face, float size, const char *s, int flags)
{
    return text_px(c, face, size, s, flags, NULL) / c->s;
}

static void blend(PxCanvas *c, int x, int y, uint32_t rgb, int a)
{
    uint32_t *p, d;
    if (a <= 0 || x < 0 || y < 0 || x >= c->w || y >= c->h) return;
    p = &c->px[(size_t)y * (size_t)c->w + (size_t)x];
    if (a >= 255) { *p = rgb; return; }
    d = *p;
    *p = (((((rgb >> 16) & 255) * a + ((d >> 16) & 255) * (255 - a)) / 255) << 16) |
         (((((rgb >> 8) & 255) * a + ((d >> 8) & 255) * (255 - a)) / 255) << 8) |
         (((rgb & 255) * a + (d & 255) * (255 - a)) / 255);
}

/* y is the top of the line: the baseline is y + the face's ascent */
float px_text(PxCanvas *c, int face, float size, float x, float y, const char *s, uint32_t rgb, int flags)
{
    Slot *sl;
    int ipx = px_u(c, size), X = px_u(c, x), Y = px_u(c, y), track, n = 0, x0;
    if (!g_font_ok[face] || ipx < 4) return 0;
    sl = slot(face, ipx);
    track = (flags & PX_TRACK) ? px_u(c, size * 0.11f) : 0;
    x0 = X;
    Y += sl->ascent;
    while (*s) {
        int cp = utf8(&s), gx, gy;
        Glyph *g;
        if (flags & PX_UPPER) cp = upper(cp);
        g = glyph(sl, cp);
        if (n) X += track;
        if (g->bm)
            for (gy = 0; gy < g->h; gy++)
                for (gx = 0; gx < g->w; gx++) {
                    int a = g->bm[gy * g->w + gx];
                    /* Pixelify Sans is drawn on a pixel grid: sharpen its edges (no soft halo) */
                    a = a < 96 ? 0 : a > 160 ? 255 : (a - 96) * 4;
                    blend(c, X + g->xo + gx, Y + g->yo + gy, rgb, a);
                }
        X += g->adv;
        n++;
    }
    return (X - x0) / c->s;
}

float px_text_fit(PxCanvas *c, int face, float size, float x, float y, const char *s, uint32_t rgb, int flags,
                  float max_w, int align)
{
    char buf[256];
    float w = px_text_w(c, face, size, s, flags);
    put(buf, sizeof buf, s);
    if (w > max_w) {
        size_t n = strlen(buf);
        while (n > 0) {
            /* back to the start of a character */
            do { n--; } while (n > 0 && ((unsigned char)buf[n] & 0xc0) == 0x80);
            buf[n] = 0;
            {
                char t[260];
                snprintf(t, sizeof t, "%s...", buf);
                if (px_text_w(c, face, size, t, flags) <= max_w) { put(buf, sizeof buf, t); break; }
            }
        }
        w = px_text_w(c, face, size, buf, flags);
    }
    if (align == PX_RIGHT) x -= w;
    else if (align == PX_CENTER) x -= w / 2;
    return px_text(c, face, size, x, y, buf, rgb, flags);
}

int px_text_wrap(PxCanvas *c, int face, float size, float x, float y, const char *s, uint32_t rgb, float max_w,
                 int max_lines, float line_h)
{
    char line[256], word[128];
    int lines = 0;
    line[0] = 0;
    while (*s && lines < max_lines) {
        size_t k = 0;
        char trial[400];
        while (*s == ' ') s++;
        while (*s && *s != ' ' && k + 1 < sizeof word) word[k++] = *s++;
        word[k] = 0;
        if (!k) break;
        snprintf(trial, sizeof trial, "%s%s%s", line, line[0] ? " " : "", word);
        if (line[0] && px_text_w(c, face, size, trial, 0) > max_w) {
            if (lines == max_lines - 1) {
                px_text_fit(c, face, size, x, y + lines * line_h, trial, rgb, 0, max_w, PX_LEFT);
                return lines + 1;
            }
            px_text(c, face, size, x, y + lines * line_h, line, rgb, 0);
            lines++;
            put(line, sizeof line, word);
        } else put(line, sizeof line, trial);
    }
    if (line[0] && lines < max_lines) {
        px_text_fit(c, face, size, x, y + lines * line_h, line, rgb, 0, max_w, PX_LEFT);
        lines++;
    }
    return lines;
}

/* ---- shapes (the Activity app's box painter) --------------------------------------------------------------- */

void px_canvas(PxCanvas *c, uint32_t *buf, int w, int h)
{
    c->px = buf;
    c->w = w;
    c->h = h;
    c->s = h / 480.0f;
}

static void fill_px(PxCanvas *c, int x, int y, int w, int h, uint32_t rgb, int a)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > c->w ? c->w : x + w, y1 = y + h > c->h ? c->h : y + h;
    int xx, yy;
    for (yy = y0; yy < y1; yy++) {
        uint32_t *row = &c->px[(size_t)yy * (size_t)c->w];
        if (a >= 255) for (xx = x0; xx < x1; xx++) row[xx] = rgb;
        else for (xx = x0; xx < x1; xx++) blend(c, xx, yy, rgb, a);
    }
}

static int row_inset(int dy, int hh, int r)
{
    double cy, d;
    int v;
    if (r <= 0 || (dy >= r && dy < hh - r)) return 0;
    cy = dy < r ? (double)r - 0.5 - dy : (double)dy - (hh - r) + 0.5;
    d = (double)r * r - cy * cy;
    if (d < 0) d = 0;
    v = (int)floor(r - sqrt(d) + 0.5);
    return v < 0 ? 0 : v;
}

static int clamp_r(int r, int w, int h)
{
    if (r > h / 2) r = h / 2;
    if (r > w / 2) r = w / 2;
    return r < 0 ? 0 : r;
}

static void round_band(PxCanvas *c, int x, int y, int w, int h, int r, int from, int to, uint32_t rgb, int a)
{
    int dy;
    r = clamp_r(r, w, h);
    if (from < 0) from = 0;
    if (to > h) to = h;
    for (dy = from; dy < to; dy++) {
        int in = row_inset(dy, h, r);
        fill_px(c, x + in, y + dy, w - 2 * in, 1, rgb, a);
    }
}

static void round_ring(PxCanvas *c, int x, int y, int w, int h, int r, int b, uint32_t rgb)
{
    int dy;
    r = clamp_r(r, w, h);
    for (dy = 0; dy < h; dy++) {
        int in = row_inset(dy, h, r);
        if (dy < b || dy >= h - b) fill_px(c, x + in, y + dy, w - 2 * in, 1, rgb, 255);
        else {
            fill_px(c, x + in, y + dy, b + (in > 0 ? 1 : 0), 1, rgb, 255);
            fill_px(c, x + w - in - b - (in > 0 ? 1 : 0), y + dy, b + (in > 0 ? 1 : 0), 1, rgb, 255);
        }
    }
}

static void dither_px(PxCanvas *c, int x, int y, int w, int h, int r)
{
    int step = (int)lroundf(c->s), dy, xx;
    if (step < 1) step = 1;
    r = clamp_r(r, w, h);
    for (dy = 0; dy < h; dy++) {
        int in = row_inset(dy, h, r), v = (dy / step) % 4;
        if (v != 0 && v != 2) continue;
        for (xx = in; xx < w - in; xx++) {
            int u = (xx / step) % 4;
            if (v == 0 && u == 0) blend(c, x + xx, y + dy, px.dither_rgb, px.dither_a);
            else if (v == 2 && u == 2) blend(c, x + xx, y + dy, px.dither_rgb, px.dither_b);
        }
    }
}

typedef struct {
    uint32_t fill, border, band_top, band_bot, inset_top, inset_bot, shadow;
    int r, border_w, band_top_h, band_bot_h, inset_top_h, inset_bot_h, dither, sdx, sdy, shadow_a, inset_top_a,
        inset_bot_a;
} Box;

static void box(PxCanvas *c, float x, float y, float w, float h, const Box *st)
{
    int X = px_u(c, x), Y = px_u(c, y), W = px_u(c, x + w) - X, H = px_u(c, y + h) - Y, R = px_u(c, (float)st->r);
    int b = px_u(c, (float)st->border_w), ix, iy, iw, ih, ir;
    if (st->shadow_a > 0)
        round_band(c, X + px_u(c, (float)st->sdx), Y + px_u(c, (float)st->sdy), W, H, R, 0, H, st->shadow,
                   st->shadow_a);
    ix = X + b; iy = Y + b; iw = W - 2 * b; ih = H - 2 * b; ir = R - b;
    if (ir < 0) ir = 0;
    if (iw > 0 && ih > 0) {
        round_band(c, ix, iy, iw, ih, ir, 0, ih, st->fill, 255);
        if (st->band_top_h > 0) round_band(c, ix, iy, iw, ih, ir, 0, px_u(c, (float)st->band_top_h), st->band_top, 255);
        if (st->band_bot_h > 0)
            round_band(c, ix, iy, iw, ih, ir, ih - px_u(c, (float)st->band_bot_h), ih, st->band_bot, 255);
        if (st->dither) dither_px(c, ix, iy, iw, ih, ir);
        if (st->inset_top_h > 0)
            round_band(c, ix, iy, iw, ih, ir, 0, px_u(c, (float)st->inset_top_h), st->inset_top, st->inset_top_a);
        if (st->inset_bot_h > 0)
            round_band(c, ix, iy, iw, ih, ir, ih - px_u(c, (float)st->inset_bot_h), ih, st->inset_bot,
                       st->inset_bot_a);
    }
    if (b > 0) round_ring(c, X, Y, W, H, R, b, st->border);
}

void px_rect(PxCanvas *c, float x, float y, float w, float h, uint32_t rgb)
{
    int X = px_u(c, x), Y = px_u(c, y);
    fill_px(c, X, Y, px_u(c, x + w) - X, px_u(c, y + h) - Y, rgb, 255);
}

void px_rect_a(PxCanvas *c, float x, float y, float w, float h, uint32_t rgb, int a)
{
    int X = px_u(c, x), Y = px_u(c, y);
    fill_px(c, X, Y, px_u(c, x + w) - X, px_u(c, y + h) - Y, rgb, a);
}

void px_round(PxCanvas *c, float x, float y, float w, float h, float r, uint32_t rgb, int a)
{
    int X = px_u(c, x), Y = px_u(c, y), W = px_u(c, x + w) - X, H = px_u(c, y + h) - Y;
    round_band(c, X, Y, W, H, px_u(c, r), 0, H, rgb, a);
}

void px_line_h(PxCanvas *c, float x, float y, float w, float t, uint32_t rgb, int a)
{
    int X = px_u(c, x), T = px_u(c, t);
    fill_px(c, X, px_u(c, y), px_u(c, x + w) - X, T < 1 ? 1 : T, rgb, a);
}

void px_dither(PxCanvas *c, float x, float y, float w, float h, float r)
{
    int X = px_u(c, x), Y = px_u(c, y);
    dither_px(c, X, Y, px_u(c, x + w) - X, px_u(c, y + h) - Y, px_u(c, r));
}

void px_triangle(PxCanvas *c, float cx, float cy, float size, int dir, uint32_t rgb)
{
    /* a step pyramid (pixel-art arrows): n columns from the tip, each two pixels taller */
    int n = px_u(c, size / 2) + 1, X = px_u(c, cx), Y = px_u(c, cy), i;
    for (i = 0; i < n; i++) {
        if (dir == -1) fill_px(c, X - n / 2 + i, Y - i, 1, 2 * i + 1, rgb, 255);
        else if (dir == 1) fill_px(c, X + n / 2 - i, Y - i, 1, 2 * i + 1, rgb, 255);
        else fill_px(c, X - i, Y + n / 2 - i, 2 * i + 1, 1, rgb, 255);
    }
}

/* the screen: its 4 px frame, the 8 px band along the top at half strength, the dither tile */
void px_screen(PxCanvas *c)
{
    int f = px_u(c, 4);
    fill_px(c, 0, 0, c->w, c->h, px.screen_bg, 255);
    fill_px(c, 0, f, c->w, px_u(c, 8), px.screen_band, 128);
    dither_px(c, 0, 0, c->w, c->h, 0);
    fill_px(c, 0, 0, c->w, f, px.screen_frame, 255);
    fill_px(c, 0, c->h - f, c->w, f, px.screen_frame, 255);
    fill_px(c, 0, 0, f, c->h, px.screen_frame, 255);
    fill_px(c, c->w - f, 0, f, c->h, px.screen_frame, 255);
}

void px_sheet(PxCanvas *c, float x, float y, float w, float h)
{
    Box b;
    memset(&b, 0, sizeof b);
    b.fill = px.sheet; b.r = 18; b.border = px.edge; b.border_w = 3; b.dither = 1;
    b.inset_top = 0xffffff; b.inset_top_h = 6; b.inset_top_a = 26;
    b.inset_bot = 0x000000; b.inset_bot_h = 8; b.inset_bot_a = 90;
    b.shadow = 0x000000; b.sdx = 4; b.sdy = 4; b.shadow_a = px.light ? 40 : 115;
    box(c, x, y, w, h, &b);
}

void px_card(PxCanvas *c, float x, float y, float w, float h, int sel, int dis)
{
    Box b;
    memset(&b, 0, sizeof b);
    b.r = 12; b.border_w = 3; b.band_top_h = 6;
    b.shadow = 0x000000; b.sdx = 3; b.sdy = 3; b.shadow_a = px.light ? 45 : 217;
    if (sel && !dis) {
        /* settings.cpp's picked row: the accent plate the shelf's cartridge stands on */
        b.fill = px.pick; b.band_top = px.pick_band; b.border = px.pick_edge;
        b.inset_bot = px.pick_low; b.inset_bot_h = 4; b.inset_bot_a = 255;
    } else {
        b.fill = px.panel_body; b.band_top = px.panel_band; b.border = sel ? px.edge : px.line;
        b.inset_bot = px.panel_low; b.inset_bot_h = 4; b.inset_bot_a = 255;
        if (dis) {
            /* greyed; picked, it keeps the bright edge so the cursor shows */
            if (!sel) b.border = px_mix(px.line, px.screen_bg, 0.45f);
            b.band_top = px_mix(px.panel_band, px.panel_body, 0.5f);
        }
    }
    box(c, x, y, w, h, &b);
}

void px_bubble(PxCanvas *c, float x, float y, float w, float h, int pointer)
{
    Box b;
    memset(&b, 0, sizeof b);
    b.r = 12; b.fill = px.bubble_body; b.border = px.line; b.border_w = 3;
    b.band_top = px.bubble_band; b.band_top_h = 10;
    b.inset_bot = px.bubble_low; b.inset_bot_h = 6; b.inset_bot_a = 255;
    b.shadow = 0x000000; b.sdx = 4; b.sdy = 4; b.shadow_a = px.light ? 40 : 115;
    box(c, x, y, w, h, &b);
    if (pointer) {
        px_triangle(c, x + w / 2, y + h + 6, 16, 2, px.line);
        px_triangle(c, x + w / 2, y + h + 2, 10, 2, px.bubble_body);
    }
}

void px_chip(PxCanvas *c, float x, float y, float w, float h, int active)
{
    Box b;
    memset(&b, 0, sizeof b);
    b.r = 8; b.border_w = 2;
    b.shadow = 0x000000; b.sdx = 2; b.sdy = 2; b.shadow_a = px.light ? 35 : 204;
    if (active) { b.fill = px.accent.base; b.border = px.accent.light; if (px.light) b.border = px.accent.dark; }
    else { b.fill = px.chip_body; b.border = px_mix(px.line, px.screen_bg, 0.1f); b.band_top = px.chip_band; b.band_top_h = 6; }
    box(c, x, y, w, h, &b);
}

float px_keycap_w(const PxCanvas *c, const char *key)
{
    return 12 + px_text_w(c, PX_MEDIUM, 16, key, PX_UPPER);
}

float px_keycap(PxCanvas *c, float x, float y, const char *key)
{
    float w = px_keycap_w(c, key);
    px_round(c, x, y, w, 23, 4, px.kbd_bg, 255);
    px_text(c, PX_MEDIUM, 16, x + 6, y + 2, key, px.kbd_ink, PX_UPPER);
    return w;
}

float px_face_w(const PxCanvas *c, const char *key, const char *label)
{
    return 15 + px_keycap_w(c, key) + 10 + px_text_w(c, PX_MEDIUM, 19, label, 0) + 18;
}

void px_face(PxCanvas *c, float x, float y, int align, const char *key, const char *label, int dis, PxRect *hit)
{
    Box b;
    float w = px_face_w(c, key, label), kx;
    if (align == PX_RIGHT) x -= w;
    else if (align == PX_CENTER) x -= w / 2;
    memset(&b, 0, sizeof b);
    b.r = 10; b.fill = px.bubble_body; b.border = dis ? px_mix(px.line, px.screen_bg, 0.45f) : px.edge; b.border_w = 3;
    b.band_top = px.bubble_band; b.band_top_h = 8;
    b.inset_bot = px.panel_low; b.inset_bot_h = 4; b.inset_bot_a = 255;
    b.shadow = 0x000000; b.sdx = 3; b.sdy = 3; b.shadow_a = px.light ? 40 : 217;
    box(c, x, y, w, 40, &b);
    kx = x + 15;
    kx += px_keycap(c, kx, y + 9, key) + 10;
    px_text(c, PX_MEDIUM, 19, kx, y + 8, label, dis ? px.dim : px.ink, 0);
    if (hit) {
        hit->x = px_u(c, x); hit->y = px_u(c, y);
        hit->w = px_u(c, x + w) - hit->x; hit->h = px_u(c, y + 40) - hit->y;
    }
}

void px_rail(PxCanvas *c, float x, float y, float h, int first, int shown, int total)
{
    float th, ty;
    px_round(c, x, y, 5, h, 2, px.rail_bg, 255);
    if (total <= 0) return;
    if (shown > total) shown = total;
    th = h * shown / total;
    if (th < 34) th = 34;
    if (th > h) th = h;
    ty = total > shown ? y + (h - th) * first / (total - shown) : y;
    px_round(c, x - 1, ty, 7, th, 2, px.edge, 255);
    px_round(c, x, ty + 1, 5, th - 2, 2, px.accent.base, 255);
    px_rect(c, x + 1, ty + 2, 3, 4, px.accent.light);
}

void px_label(PxCanvas *c, float x, float y, const char *s, uint32_t rgb)
{
    px_text(c, PX_REGULAR, 13, x, y, s, rgb, PX_UPPER | PX_TRACK);
}

void px_value_arrows(PxCanvas *c, float right, float y, float size, const char *value, uint32_t rgb, int left,
                     int right_on)
{
    float w = px_text_w(c, PX_MEDIUM, size, value, 0), x = right - 12 - w;
    if (right_on) px_triangle(c, right - 4, y + size * 0.55f, 8, 1, rgb);
    px_text(c, PX_MEDIUM, size, x, y, value, rgb, 0);
    if (left) px_triangle(c, x - 12, y + size * 0.55f, 8, -1, rgb);
}

/* ---- the status row ------------------------------------------------------------------------------------------ */

int px_battery(int *charging)
{
    DIR *d = opendir("/sys/class/power_supply");
    struct dirent *e;
    int pct = -1, chg = 0;
    if (!d) { if (charging) *charging = 0; return -1; }
    while ((e = readdir(d))) {
        char path[320], buf[32];
        FILE *f;
        if (e->d_name[0] == '.') continue;
        snprintf(path, sizeof path, "/sys/class/power_supply/%s/capacity", e->d_name);
        f = fopen(path, "r");
        if (f) {
            if (fgets(buf, sizeof buf, f) && pct < 0) pct = atoi(buf);
            fclose(f);
        }
        snprintf(path, sizeof path, "/sys/class/power_supply/%s/online", e->d_name);
        f = fopen(path, "r");
        if (f) {
            if (fgets(buf, sizeof buf, f) && atoi(buf) == 1) chg = 1;   /* a charger plugged in counts as charging */
            fclose(f);
        }
    }
    closedir(d);
    if (charging) *charging = chg;
    return pct;
}

void px_clock(char *buf, int n, const PxSettings *st)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    if (st && !strcmp(st->clock, "12h")) {
        int h = tm.tm_hour % 12;
        snprintf(buf, (size_t)n, "%d:%02d %s", h ? h : 12, tm.tm_min, tm.tm_hour < 12 ? "AM" : "PM");
    } else snprintf(buf, (size_t)n, "%02d:%02d", tm.tm_hour, tm.tm_min);
}

static void battery_icon(PxCanvas *c, float x, float y, int pct, int charging)
{
    float w = 26, h = 14, f;
    px_rect(c, x, y, w, h, px.muted);
    px_rect(c, x + 2, y + 2, w - 4, h - 4, px.screen_bg);
    px_rect(c, x + w, y + 4, 3, h - 8, px.muted);
    if (pct < 0) return;
    f = (w - 6) * (pct > 100 ? 100 : pct) / 100.0f;
    px_rect(c, x + 3, y + 3, f, h - 6, pct <= 15 && !charging ? px.red : charging ? px.green : px.ink);
}

void px_status(PxCanvas *c, const char *brand, int battery, int charging, const char *clock)
{
    float bw = px_text_w(c, PX_MEDIUM, 13, brand, PX_UPPER | PX_TRACK) + 24, x;
    Box b;
    memset(&b, 0, sizeof b);
    b.r = 6; b.fill = px.brand_bg; b.border = px.accent.base; b.border_w = 2;
    box(c, 18, 18, bw, 26, &b);
    px_text(c, PX_MEDIUM, 13, 30, 23, brand, px.accent.base, PX_UPPER | PX_TRACK);
    x = 622;
    if (clock && clock[0]) {
        x -= px_text_w(c, PX_MEDIUM, 19, clock, 0);
        px_text(c, PX_MEDIUM, 19, x, 20, clock, px.ink, 0);
        x -= 18;
    }
    if (battery >= 0) {
        char pc[16];
        snprintf(pc, sizeof pc, "%d%%", battery);
        x -= 29;
        battery_icon(c, x, 24, battery, charging);
        x -= 8 + px_text_w(c, PX_REGULAR, 16, pc, 0);
        px_text(c, PX_REGULAR, 16, x, 22, pc, px.muted, 0);
    }
}

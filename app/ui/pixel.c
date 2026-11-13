/* pixel: the menu's look for apps that draw into a framebuffer. See pixel.h.
   Palettes, accents and shape recipes ported from JorreFog/BananaLinux (MIT, Copyright (c) 2026 JorreFog and the
   Banana contributors): ui/src/settings_pal.cpp, ui/rnds/RndsUI.cpp, store/apps/activity (DESIGN.md and its box
   painter). */
#include "pixel.h"
#include "stb_truetype.h"

#include <dirent.h>
#include <math.h>
#include <ctype.h>
#include <stddef.h>
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

/* ---- themes: banana/themes/<name>.json (settings.conf's theme = user:<name>) ----------------------------------
   The same reading as banana-ui's theme.cpp: "base" (dark or light), "accent" (a colour, one of the names, or
   {base, light, dark, band, low}), "colors" (the palette's colours by name), "background.dither". The rest (fonts,
   pictures, screens, sounds) is the menu's own. */

#define TH_MAX 96
typedef struct { char k[48], v[48]; } ThKv;
typedef struct { ThKv kv[TH_MAX]; int n; const char *s; size_t i, len; int ok; } Th;

static void th_ws(Th *t)
{
    while (t->i < t->len) {
        char ch = t->s[t->i];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || ch == ',' || ch == ':') { t->i++; continue; }
        if (ch == '/' && t->i + 1 < t->len && t->s[t->i + 1] == '/') {
            while (t->i < t->len && t->s[t->i] != '\n') t->i++;
            continue;
        }
        if (ch == '/' && t->i + 1 < t->len && t->s[t->i + 1] == '*') {
            const char *e = strstr(t->s + t->i + 2, "*/");
            t->i = e ? (size_t)(e - t->s) + 2 : t->len;
            continue;
        }
        break;
    }
}

static void th_str(Th *t, char *out, size_t n)
{
    size_t k = 0;
    if (t->s[t->i] != '"') { t->ok = 0; return; }
    for (t->i++; t->i < t->len && t->s[t->i] != '"'; t->i++) {
        char ch = t->s[t->i];
        if (ch == '\\' && t->i + 1 < t->len) ch = t->s[++t->i];
        if (out && k + 1 < n) out[k++] = ch;
    }
    if (out && n) out[k] = 0;
    if (t->i >= t->len) t->ok = 0;
    t->i++;
}

static int th_keep(const char *k)
{
    return !strcmp(k, "base") || !strcmp(k, "name") || !strncmp(k, "accent", 6) || !strncmp(k, "colors.", 7) ||
           !strcmp(k, "background.dither");
}

static void th_put(Th *t, const char *k, const char *v)
{
    if (!th_keep(k) || t->n >= TH_MAX) return;
    put(t->kv[t->n].k, sizeof t->kv[t->n].k, k);
    put(t->kv[t->n].v, sizeof t->kv[t->n].v, v);
    t->n++;
}

static void th_value(Th *t, const char *key, int depth);

static void th_object(Th *t, const char *prefix, int depth)
{
    t->i++;  /* { */
    while (t->ok) {
        char k[64], full[128];
        th_ws(t);
        if (t->i >= t->len) { t->ok = 0; return; }
        if (t->s[t->i] == '}') { t->i++; return; }
        th_str(t, k, sizeof k);
        if (!t->ok) return;
        snprintf(full, sizeof full, "%s%s%s", prefix, prefix[0] ? "." : "", k);
        th_value(t, full, depth + 1);
    }
}

static void th_value(Th *t, const char *key, int depth)
{
    th_ws(t);
    if (t->i >= t->len || depth > 32) { t->ok = 0; return; }
    if (t->s[t->i] == '"') {
        char v[64];
        th_str(t, v, sizeof v);
        th_put(t, key, v);
    } else if (t->s[t->i] == '{') th_object(t, key, depth);
    else if (t->s[t->i] == '[') {
        int d = 0;
        do {
            char ch = t->s[t->i];
            if (ch == '"') { th_str(t, NULL, 0); continue; }
            if (ch == '[') d++;
            else if (ch == ']') d--;
            t->i++;
        } while (t->ok && t->i < t->len && d > 0);
    } else {
        char v[48];
        size_t k = 0;
        while (t->i < t->len && (isalnum((unsigned char)t->s[t->i]) || strchr(".-+", t->s[t->i]))) {
            if (k + 1 < sizeof v) v[k++] = t->s[t->i];
            t->i++;
        }
        v[k] = 0;
        if (!k) { t->ok = 0; return; }
        th_put(t, key, v);
    }
}

static const char *th_get(const Th *t, const char *k)
{
    int i;
    for (i = 0; i < t->n; i++)
        if (!strcmp(t->kv[i].k, k)) return t->kv[i].v;
    return "";
}

/* "#rgb", "#rrggbb", "#rrggbbaa": the colour and its opacity */
static int th_color(const char *v, uint32_t *rgb, int *alpha)
{
    char h[9];
    size_t n, i;
    unsigned long x;
    if (v[0] != '#') return 0;
    v++;
    n = strlen(v);
    for (i = 0; i < n; i++) if (!isxdigit((unsigned char)v[i])) return 0;
    if (n == 3) { snprintf(h, sizeof h, "%c%c%c%c%c%cff", v[0], v[0], v[1], v[1], v[2], v[2]); }
    else if (n == 6) snprintf(h, sizeof h, "%sff", v);
    else if (n == 8) snprintf(h, sizeof h, "%s", v);
    else return 0;
    x = strtoul(h, NULL, 16);
    *rgb = (uint32_t)(x >> 8) & 0xffffff;
    if (alpha) *alpha = (int)(x & 0xff);
    return 1;
}

/* the theme file a settings value names ("user:<name>": the card's banana/themes first, then the system's) */
static int theme_path(const char *value, char *out, size_t n)
{
    const char *card = getenv("BANANA_THEMES"), *image = getenv("PIXEL_UI_THEMES_IMAGE");
    FILE *f;
    if (!strncmp(value, "file:", 5)) { put(out, n, value + 5); return 1; }
    if (strncmp(value, "user:", 5) || !value[5] || strchr(value + 5, '/')) return 0;
    snprintf(out, n, "%s/%s.json", card && card[0] ? card : "/data/banana/themes", value + 5);
    if ((f = fopen(out, "r"))) { fclose(f); return 1; }
    snprintf(out, n, "%s/%s.json", image && image[0] ? image : "/usr/share/banana/themes", value + 5);
    return 1;
}

static int theme_read(const char *value, Th *t)
{
    char path[512];
    unsigned char *b;
    FILE *f;
    long len;
    memset(t, 0, sizeof *t);
    if (!theme_path(value, path, sizeof path)) return 0;
    f = fopen(path, "rb");
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) || (len = ftell(f)) < 2 || len > 4 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) {
        fclose(f);
        return 0;
    }
    b = malloc((size_t)len + 1);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(b); return 0; }
    fclose(f);
    b[len] = 0;
    t->s = (const char *)b;
    t->len = (size_t)len;
    t->ok = 1;
    th_ws(t);
    if (t->i < t->len && t->s[t->i] == '{') th_object(t, "", 0);
    else t->ok = 0;
    free(b);
    t->s = NULL;
    return t->ok;
}

static void th_set(const Th *t, const char *name, uint32_t *field)
{
    uint32_t c;
    if (th_color(th_get(t, name), &c, NULL)) *field = c;
}

void px_pal_make(PxPal *p, const PxSettings *st)
{
    size_t i;
    PxAccent a = ACCENTS[0].a;
    Th th;
    int themed = 0, light;
    uint32_t c, sheet_base;
    float sheet_mix;
    const char *acc;
    for (i = 0; i < sizeof ACCENTS / sizeof ACCENTS[0]; i++)
        if (st && !strcmp(st->accent, ACCENTS[i].name)) a = ACCENTS[i].a;
    g_accent_text = st && !strcmp(st->accent_text, "dark") ? 1 : st && !strcmp(st->accent_text, "light") ? 2 : 0;
    memset(p, 0, sizeof *p);
    light = st && !strcmp(st->theme, "light");
    if (st && theme_read(st->theme, &th)) {
        themed = 1;
        light = !strcmp(th_get(&th, "base"), "light");
        /* the theme's own accent wins over Settings' */
        acc = th_get(&th, "accent");
        if (th_color(acc, &c, NULL)) {
            a.base = c; a.light = px_mix(c, 0xffffff, 0.6f); a.dark = px_mix(c, 0, 0.62f);
            a.band = px_mix(c, 0xffffff, 0.3f); a.low = px_mix(c, 0, 0.2f);
        } else if (th_color(th_get(&th, "accent.base"), &c, NULL)) {
            a.base = c;
            a.light = px_mix(c, 0xffffff, 0.6f); a.dark = px_mix(c, 0, 0.62f);
            a.band = px_mix(c, 0xffffff, 0.3f); a.low = px_mix(c, 0, 0.2f);
            th_set(&th, "accent.light", &a.light); th_set(&th, "accent.dark", &a.dark);
            th_set(&th, "accent.band", &a.band); th_set(&th, "accent.low", &a.low);
        } else if (acc[0]) {
            for (i = 0; i < sizeof ACCENTS / sizeof ACCENTS[0]; i++)
                if (!strcmp(acc, ACCENTS[i].name)) a = ACCENTS[i].a;
        }
    } else if (st && !strncmp(st->theme, "user:", 5)) {
        /* a theme that can't be read: the menu shows Dark (theme.cpp) */
        light = 0;
    }
    p->accent = a;
    p->light = light;
    /* RndsUI.cpp's darkPal() and lightPal() */
    if (!light) {
        p->ink = 0xf1ede2; p->muted = 0xa39d8e; p->line = 0x8a8374; p->edge = 0xe8e2d3;
        p->screen_bg = 0x17161c; p->screen_frame = 0x0b0a0d; p->screen_band = 0x2e2b33;
        p->panel_band = 0x3a3540; p->panel_body = 0x17161c; p->panel_low = 0x0b0a0d;
        p->bubble_band = 0x38333b; p->bubble_body = 0x191820; p->bubble_low = 0x0f0e13;
        p->chip_band = 0x2e2b33; p->chip_body = 0x121117; p->rail_bg = 0x100f14; p->rule = 0x45403d;
        p->brand_bg = 0x262219; p->kbd_bg = 0xf1ede2; p->kbd_ink = 0x17161c;
        p->red = 0xff3b4a;
        p->dither_rgb = 0xffffff; p->dither_a = 36; p->dither_b = 18;
        sheet_base = 0x15141a; sheet_mix = 0.34f;
        p->pick_edge = 0xffffff;
    } else {
        p->ink = 0x2a2620; p->muted = 0x7a7262; p->line = 0xa69d8c; p->edge = 0x3a342c;
        p->screen_bg = 0xf3efe6; p->screen_frame = 0xd4ccbd; p->screen_band = 0xfdfcf9;
        p->panel_band = 0xffffff; p->panel_body = 0xf6f2ea; p->panel_low = 0xd8d0c0;
        p->bubble_band = 0xffffff; p->bubble_body = 0xfaf7f0; p->bubble_low = 0xddd5c6;
        p->chip_band = 0xffffff; p->chip_body = 0xf6f2ea; p->rail_bg = 0xe6dfd0; p->rule = 0xd6cfc0;
        p->brand_bg = 0x262219; p->kbd_bg = 0x3a342c; p->kbd_ink = 0xf2f6fa;
        p->red = 0xe23b4a;
        p->dither_rgb = 0x3a2e14; p->dither_a = 18; p->dither_b = 9;
        sheet_base = 0xfdfbf6; sheet_mix = 0.3f;
        p->pick_edge = a.dark;
    }
    if (themed) {
        static const struct { const char *name; size_t off; } MAP[] = {
            { "colors.ink", offsetof(PxPal, ink) }, { "colors.muted", offsetof(PxPal, muted) },
            { "colors.line", offsetof(PxPal, line) }, { "colors.edge", offsetof(PxPal, edge) },
            { "colors.red", offsetof(PxPal, red) }, { "colors.screenBg", offsetof(PxPal, screen_bg) },
            { "colors.screenFrame", offsetof(PxPal, screen_frame) }, { "colors.screenBand", offsetof(PxPal, screen_band) },
            { "colors.panelBand", offsetof(PxPal, panel_band) }, { "colors.panelBody", offsetof(PxPal, panel_body) },
            { "colors.panelLow", offsetof(PxPal, panel_low) }, { "colors.bubbleBand", offsetof(PxPal, bubble_band) },
            { "colors.bubbleBody", offsetof(PxPal, bubble_body) }, { "colors.bubbleLow", offsetof(PxPal, bubble_low) },
            { "colors.chipBand", offsetof(PxPal, chip_band) }, { "colors.chipBody", offsetof(PxPal, chip_body) },
            { "colors.railBg", offsetof(PxPal, rail_bg) }, { "colors.rule", offsetof(PxPal, rule) },
            { "colors.brandBg", offsetof(PxPal, brand_bg) }, { "colors.kbdBg", offsetof(PxPal, kbd_bg) },
            { "colors.kbdInk", offsetof(PxPal, kbd_ink) },
        };
        int al;
        for (i = 0; i < sizeof MAP / sizeof MAP[0]; i++) th_set(&th, MAP[i].name, (uint32_t *)((char *)p + MAP[i].off));
        th_set(&th, "colors.sheetBase", &sheet_base);
        if (th_color(th_get(&th, "colors.ditherA"), &c, &al)) { p->dither_rgb = c; p->dither_a = al; }
        if (th_color(th_get(&th, "colors.ditherB"), &c, &al)) p->dither_b = al;
        if (!strcmp(th_get(&th, "background.dither"), "false")) p->dither_a = p->dither_b = 0;
    }
    p->dim = px_mix(p->muted, p->panel_body, 0.25f);
    p->sheet = px_mix(sheet_base, a.base, sheet_mix);
    /* the success green and the warning red, readable on the sheet */
    p->green = light ? 0x2f9e4f : 0x4cc76a;
    p->warn = p->red;
    for (i = 0; i < 12 && contrast(p->warn, p->sheet) < 4.0f; i++) p->warn = px_mix(p->warn, light ? 0 : 0xffffff, 0.15f);
    p->pick = a.base; p->pick_band = a.band; p->pick_low = a.low;
    p->on_pick = px_ink_on(a.base); p->on_pick_sub = sub_ink_on(a.base);
    if (themed) { th_set(&th, "colors.onPick", &p->on_pick); th_set(&th, "colors.onPickAccent", &p->on_pick_sub); }
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
    int face, ipx, used;        /* ipx: the em size in 1/8 pixels */
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
    g_slot[s].scale = stbtt_ScaleForMappingEmToPixels(&g_font[face], ipx / 8.0f);
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

/* the em size in 1/8 pixels. banana-ui sets Pixelify Sans by its em (a CSS font size); on the RG DS Plus these apps
   keep the size they were tuned at there (5/6 of it), on the RG DS (1x) the full em, never under 12 px, so the
   small labels stay readable */
static int em8(const PxCanvas *c, float size)
{
    float em = c->s < 1.3f ? size * c->s : size * c->s / 1.2f;
    if (c->s < 1.3f && em < 12) em = 12;
    return (int)lroundf(em * 8);
}

/* at 1x the small text is set in Medium: Regular's one-pixel strokes go grey there */
static int face_for(const PxCanvas *c, int face, int e8)
{
    return c->s < 1.3f && e8 < 15 * 8 ? PX_MEDIUM : face;
}

static int track_px(const PxCanvas *c, float size, int flags)
{
    int t;
    if (!(flags & PX_TRACK)) return 0;
    t = px_u(c, size * 0.11f);
    return t < 1 ? 1 : t;
}

static int kern(Slot *sl, int a, int b)
{
    if (!a || !b) return 0;
    return (int)lroundf(stbtt_GetCodepointKernAdvance(&g_font[sl->face], a, b) * sl->scale);
}

static int text_px(const PxCanvas *c, int face, float size, const char *s, int flags, int *count)
{
    Slot *sl;
    int e8 = em8(c, size), w = 0, n = 0, track = track_px(c, size, flags), prev = 0;
    face = face_for(c, face, e8);
    if (!g_font_ok[face] || e8 < 32) { if (count) *count = 0; return 0; }
    sl = slot(face, e8);
    while (*s) {
        int cp = utf8(&s);
        if (flags & PX_UPPER) cp = upper(cp);
        w += kern(sl, prev, cp) + glyph(sl, cp)->adv + (n ? track : 0);
        prev = cp;
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

/* y is the top of the line: the baseline is y + the face's ascent. Glyphs land on whole pixels with their plain
   coverage (banana-ui: no hinting, whole-pixel origins); only the faintest fringe is dropped */
float px_text(PxCanvas *c, int face, float size, float x, float y, const char *s, uint32_t rgb, int flags)
{
    Slot *sl;
    int e8 = em8(c, size), X = px_u(c, x), Y = px_u(c, y), track, n = 0, x0, prev = 0;
    face = face_for(c, face, e8);
    if (!g_font_ok[face] || e8 < 32) return 0;
    sl = slot(face, e8);
    track = track_px(c, size, flags);
    x0 = X;
    Y += sl->ascent;
    while (*s) {
        int cp = utf8(&s), gx, gy;
        Glyph *g;
        if (flags & PX_UPPER) cp = upper(cp);
        g = glyph(sl, cp);
        X += kern(sl, prev, cp);
        if (n) X += track;
        if (g->bm)
            for (gy = 0; gy < g->h; gy++)
                for (gx = 0; gx < g->w; gx++) {
                    int a = g->bm[gy * g->w + gx];
                    a = a < 40 ? 0 : a > 215 ? 255 : (a - 40) * 255 / 175;
                    blend(c, X + g->xo + gx, Y + g->yo + gy, rgb, a);
                }
        X += g->adv;
        prev = cp;
        n++;
    }
    return (X - x0) / c->s;
}

/* text that ends in an animated "." ".." "...": laid out as if the three were there, so nothing moves */
float px_text_dots(PxCanvas *c, int face, float size, float x, float y, const char *s, int dots, uint32_t rgb,
                   int flags, int align)
{
    char full[200], now[200];
    float w;
    snprintf(full, sizeof full, "%s...", s);
    snprintf(now, sizeof now, "%s%.*s", s, dots < 0 ? 0 : dots > 3 ? 3 : dots, "...");
    w = px_text_w(c, face, size, full, flags);
    if (align == PX_RIGHT) x -= w;
    else if (align == PX_CENTER) x -= w / 2;
    px_text(c, face, size, x, y, now, rgb, flags);
    return w;
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

/* the state badge on the top panel: a lamp and a word in capitals, green (on), grey (off), the accent (on its way)
   or red (can't) */
static float badge_w(const PxCanvas *c, float h, const char *text, int dots)
{
    char full[200];
    snprintf(full, sizeof full, "%s%s", text, dots >= 0 ? "..." : "");
    return h * 0.5f + h * 0.36f + h * 0.3f + px_text_w(c, PX_MEDIUM, h * 0.5f, full, PX_UPPER | PX_TRACK) + h * 0.5f;
}

float px_badge_w(const PxCanvas *c, float h, const char *text)
{
    return badge_w(c, h, text, -1);
}

float px_badge(PxCanvas *c, float x, float y, float h, const char *text, int kind)
{
    return px_badge_dots(c, x, y, h, text, -1, kind);
}

float px_badge_dots(PxCanvas *c, float x, float y, float h, const char *text, int dots, int kind)
{
    Box b;
    float w = badge_w(c, h, text, dots), size = h * 0.5f, lamp = h * 0.36f, lx = x + h * 0.5f;
    uint32_t fill, band, low, edge, lamp_on, ink;
    switch (kind) {
    case PX_BADGE_ON:
        fill = px.green; band = px_mix(px.green, 0xffffff, 0.3f); low = px_mix(px.green, 0, 0.3f);
        lamp_on = px_mix(px.green, 0xffffff, 0.75f);
        break;
    case PX_BADGE_BUSY:
        fill = px.accent.base; band = px.accent.band; low = px.accent.low; lamp_on = px.accent.light;
        break;
    case PX_BADGE_WARN:
        fill = px.red; band = px_mix(px.red, 0xffffff, 0.3f); low = px_mix(px.red, 0, 0.3f);
        lamp_on = px_mix(px.red, 0xffffff, 0.7f);
        break;
    default:
        fill = px.light ? 0xb9b2a5 : 0x56525c; band = px_mix(fill, 0xffffff, 0.22f); low = px_mix(fill, 0, 0.28f);
        lamp_on = px.light ? 0x8a8478 : 0x2a282e;
        break;
    }
    edge = px.light ? px_mix(fill, 0, 0.55f) : px_mix(fill, 0xffffff, 0.55f);
    ink = px_ink_on(fill);
    memset(&b, 0, sizeof b);
    b.r = 10; b.border_w = 3; b.fill = fill; b.border = edge;
    b.band_top = band; b.band_top_h = (int)(h * 0.2f);
    b.inset_bot = low; b.inset_bot_h = 4; b.inset_bot_a = 255;
    b.shadow = 0x000000; b.sdx = 3; b.sdy = 3; b.shadow_a = px.light ? 45 : 200;
    box(c, x, y, w, h, &b);
    /* the lamp: a lit square with a dark rim (off: unlit) */
    px_rect(c, lx - 2, y + (h - lamp) / 2 - 2, lamp + 4, lamp + 4, px_mix(low, 0, 0.35f));
    px_rect(c, lx, y + (h - lamp) / 2, lamp, lamp, lamp_on);
    if (kind != PX_BADGE_OFF) px_rect(c, lx + 2, y + (h - lamp) / 2 + 2, lamp * 0.35f, lamp * 0.35f, 0xffffff);
    if (dots >= 0)
        px_text_dots(c, PX_MEDIUM, size, lx + lamp + h * 0.3f, y + (h - size * 1.2f) / 2 + 1, text, dots, ink,
                     PX_UPPER | PX_TRACK, PX_LEFT);
    else px_text(c, PX_MEDIUM, size, lx + lamp + h * 0.3f, y + (h - size * 1.2f) / 2 + 1, text, ink, PX_UPPER | PX_TRACK);
    return w;
}

static float g_face_size = 19;    /* the face buttons' label size (px_faces makes it smaller to fit a row) */

/* a one-letter key in 16, a word (SELECT, START) in 12 */
static float cap_size(const char *key)
{
    return strlen(key) > 1 ? 12 : 16;
}

float px_keycap_w(const PxCanvas *c, const char *key)
{
    return 12 + px_text_w(c, PX_MEDIUM, cap_size(key), key, PX_UPPER);
}

float px_keycap(PxCanvas *c, float x, float y, const char *key)
{
    float w = px_keycap_w(c, key), sz = cap_size(key);
    px_round(c, x, y, w, 23, 4, px.kbd_bg, 255);
    px_text(c, PX_MEDIUM, sz, x + 6, y + 2 + (16 - sz) * 0.55f, key, px.kbd_ink, PX_UPPER);
    return w;
}

float px_face_w(const PxCanvas *c, const char *key, const char *label)
{
    return 13 + px_keycap_w(c, key) + 9 + px_text_w(c, PX_MEDIUM, g_face_size, label, 0) + 15;
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
    kx = x + 13;
    kx += px_keycap(c, kx, y + 9, key) + 9;
    px_text(c, PX_MEDIUM, g_face_size, kx, y + 8 + (19 - g_face_size) * 0.5f, label, dis ? px.dim : px.ink, 0);
    if (hit) {
        hit->x = px_u(c, x); hit->y = px_u(c, y);
        hit->w = px_u(c, x + w) - hit->x; hit->h = px_u(c, y + 40) - hit->y;
    }
}

void px_faces(PxCanvas *c, float y, int n, int left, const PxFace *f, PxRect *hits)
{
    float total, x;
    int i;
    for (g_face_size = 19; g_face_size > 14; g_face_size -= 1) {
        total = 0;
        for (i = 0; i < n; i++) total += px_face_w(c, f[i].key, f[i].label) + (i ? 8 : 0);
        if (n > left && left > 0) total += 16;   /* the gap between the two groups */
        if (total <= 604) break;
    }
    x = 18;
    for (i = 0; i < left && i < n; i++) {
        px_face(c, x, y, PX_LEFT, f[i].key, f[i].label, f[i].dis, hits ? &hits[i] : NULL);
        x += px_face_w(c, f[i].key, f[i].label) + 8;
    }
    x = 622;
    for (i = n - 1; i >= left; i--) {
        px_face(c, x, y, PX_RIGHT, f[i].key, f[i].label, f[i].dis, hits ? &hits[i] : NULL);
        x -= px_face_w(c, f[i].key, f[i].label) + 8;
    }
    g_face_size = 19;
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

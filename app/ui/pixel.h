/* pixel: a small drawing kit for BananaOS apps that draw straight into a 32-bit framebuffer.
   It draws in the menu's look (ROCKNIXDS Pixel as banana-ui has it): a dithered screen, a rounded sheet with a
   bright 3 px edge and light/dark bands, banded cards, the accent plate for the picked row, drawn key caps and
   Pixelify Sans with spaced capitals for labels.

   Everything is laid out on the menu's 640x480 grid ("units") and scaled to the panel: 1x on the RG DS
   (640x480), 1.6x on the RG DS Plus (1024x768).

   The palettes, the accent table and the shape recipes are ported from JorreFog/BananaLinux (MIT, Copyright (c)
   2026 JorreFog and the Banana contributors): ui/src/settings_pal.cpp, ui/rnds/RndsUI.cpp and
   store/apps/activity/DESIGN.md. This file is shared as it is by bananads-lan and bananaos-direct-link. */
#ifndef PIXEL_H
#define PIXEL_H

#include <stdint.h>

typedef struct {
    uint32_t *px;               /* XRGB8888, w * h, row after row */
    int w, h;
    float s;                    /* units -> pixels: h / 480 */
} PxCanvas;

typedef struct { uint32_t base, light, dark, band, low; } PxAccent;

typedef struct {
    int light;
    uint32_t ink, muted, line, edge, dim;
    uint32_t screen_bg, screen_frame, screen_band;
    uint32_t panel_band, panel_body, panel_low;
    uint32_t bubble_band, bubble_body, bubble_low;
    uint32_t chip_band, chip_body, rail_bg, rule, brand_bg;
    uint32_t kbd_bg, kbd_ink;
    uint32_t red, green, warn, sheet;   /* warn: red text that reads on the sheet */
    uint32_t pick, pick_band, pick_low, pick_edge, on_pick, on_pick_sub;
    uint32_t dither_rgb;
    int dither_a, dither_b;     /* alpha of the 4x4 tile's two dots, 0-255 */
    PxAccent accent;
} PxPal;

/* what /data/banana/settings.conf says (theme, accent, accent_text, language, clock) */
typedef struct {
    char theme[96], accent[16], accent_text[8], language[16], clock[8];
} PxSettings;

enum { PX_REGULAR = 0, PX_MEDIUM = 1 };
enum { PX_LEFT = 0, PX_CENTER = 1, PX_RIGHT = 2 };
/* text flags */
#define PX_UPPER 1               /* in capitals */
#define PX_TRACK 2               /* spaced (the micro labels' tracking) */

typedef struct { int x, y, w, h; } PxRect;   /* in pixels, for touch */

/* settings: path NULL is /data/banana/settings.conf (PIXEL_UI_SETTINGS overrides it); missing keys are the
   menu's defaults (dark, yellow, auto, English, 24h) */
void px_settings_read(const char *path, PxSettings *st);
/* the palette: settings.conf's theme (dark, light, or user:<name>: banana/themes/<name>.json, its base, accent and
   colours as banana-ui reads them) and accent */
void px_pal_make(PxPal *p, const PxSettings *st);
extern PxPal px;                 /* the palette in use */

/* fonts: Pixelify Sans Regular and Medium from dir (NULL: PIXEL_UI_FONTS, else /usr/share/banana/rnds/fonts) */
int px_fonts_load(const char *dir);
void px_fonts_free(void);

/* translations: a table of {english, spanish} pairs ended by {NULL, NULL}; px_tr gives the line in the language
   settings.conf names ("es": Spanish), else the English text as it is */
typedef struct { const char *en, *es; } PxTr;
void px_lang_set(const char *code, const PxTr *table);
const char *px_tr(const char *en);
int px_lang_is(const char *code);

/* the canvas */
void px_canvas(PxCanvas *c, uint32_t *buf, int w, int h);
int px_u(const PxCanvas *c, float units);    /* units -> pixels */

/* drawing (every position and size in units) */
void px_rect(PxCanvas *c, float x, float y, float w, float h, uint32_t rgb);
void px_rect_a(PxCanvas *c, float x, float y, float w, float h, uint32_t rgb, int a);
void px_round(PxCanvas *c, float x, float y, float w, float h, float r, uint32_t rgb, int a);
void px_line_h(PxCanvas *c, float x, float y, float w, float t, uint32_t rgb, int a);
void px_dither(PxCanvas *c, float x, float y, float w, float h, float r);
void px_triangle(PxCanvas *c, float cx, float cy, float size, int dir, uint32_t rgb); /* dir -1 left, 1 right, 2 down */

/* text: size in units; returns the width in units */
float px_text(PxCanvas *c, int face, float size, float x, float y, const char *s, uint32_t rgb, int flags);
float px_text_w(const PxCanvas *c, int face, float size, const char *s, int flags);
/* text ending in an animated 0-3 dots, laid out as if all three were there (it doesn't move); align as px_text_fit */
float px_text_dots(PxCanvas *c, int face, float size, float x, float y, const char *s, int dots, uint32_t rgb,
                   int flags, int align);
/* text cut with "..." to max_w units */
float px_text_fit(PxCanvas *c, int face, float size, float x, float y, const char *s, uint32_t rgb, int flags,
                  float max_w, int align);
/* wrapped text; returns the lines used */
int px_text_wrap(PxCanvas *c, int face, float size, float x, float y, const char *s, uint32_t rgb, float max_w,
                 int max_lines, float line_h);

/* the menu's parts */
void px_screen(PxCanvas *c);                                     /* background: frame, top band, dither */
void px_sheet(PxCanvas *c, float x, float y, float w, float h);  /* the tinted hero panel */
/* a card (a list row): sel draws the accent plate; dis greys it */
void px_card(PxCanvas *c, float x, float y, float w, float h, int sel, int dis);
void px_bubble(PxCanvas *c, float x, float y, float w, float h, int pointer);
void px_chip(PxCanvas *c, float x, float y, float w, float h, int active);
/* the state badge (top panel): h units high; kind PX_BADGE_ON green, _OFF grey, _BUSY the accent, _WARN red */
enum { PX_BADGE_OFF = 0, PX_BADGE_ON = 1, PX_BADGE_BUSY = 2, PX_BADGE_WARN = 3 };
float px_badge(PxCanvas *c, float x, float y, float h, const char *text, int kind);
float px_badge_w(const PxCanvas *c, float h, const char *text);
/* the same with an animated "..." after the word (0-3 dots), its width kept for all three */
float px_badge_dots(PxCanvas *c, float x, float y, float h, const char *text, int dots, int kind);
/* a key cap ("A", "B", "START"); returns its width */
float px_keycap(PxCanvas *c, float x, float y, const char *key);
float px_keycap_w(const PxCanvas *c, const char *key);
/* a face button: a key cap and its label, 40 high; align PX_LEFT/RIGHT/CENTER around x; dis greys it.
   Its area in pixels goes to *hit (may be NULL) */
void px_face(PxCanvas *c, float x, float y, int align, const char *key, const char *label, int dis, PxRect *hit);
float px_face_w(const PxCanvas *c, const char *key, const char *label);
/* the face row along the bottom (y): the first `left` from the left edge, the rest against the right one, their
   labels made smaller (19 down to 15) until all of them fit; hits[i] (may be NULL) gets each one's area */
typedef struct { const char *key, *label; int dis; } PxFace;
void px_faces(PxCanvas *c, float y, int n, int left, const PxFace *f, PxRect *hits);
/* the scroll rail on the right: first shown, shown, total */
void px_rail(PxCanvas *c, float x, float y, float h, int first, int shown, int total);
/* the status row (y 18-46): the app's brand plate on the left, battery and clock on the right */
void px_status(PxCanvas *c, const char *brand, int battery, int charging, const char *clock);
/* a section label (13, capitals, spaced, muted) */
void px_label(PxCanvas *c, float x, float y, const char *s, uint32_t rgb);
/* "<  value  >" in a row: arrows only where there is somewhere to go */
void px_value_arrows(PxCanvas *c, float right, float y, float size, const char *value, uint32_t rgb, int left,
                     int right_on);

/* what the status row shows */
int px_battery(int *charging);                 /* percent, -1 without one */
void px_clock(char *buf, int n, const PxSettings *st);

/* colours */
uint32_t px_mix(uint32_t a, uint32_t b, float t);
uint32_t px_ink_on(uint32_t rgb);

#endif

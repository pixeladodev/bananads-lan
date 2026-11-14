#define _GNU_SOURCE
#include "pixel_image.h"

#include <ctype.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_WRITE
#define STBI_MAX_DIMENSIONS 4096
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "third_party/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

void px_image_free(PxImage *img)
{
    free(img->px);
    memset(img, 0, sizeof *img);
}

/* shrunk by averaging every source pixel that falls in a target one (a cover read down to a tile stays smooth) */
int px_image_load(const char *path, int max_w, int max_h, PxImage *img)
{
    memset(img, 0, sizeof *img);
    int w, h, n;
    unsigned char *d = stbi_load(path, &w, &h, &n, 4);
    if (!d) return 0;
    float k = 1.0f;
    if (max_w > 0 && w > max_w) k = (float)max_w / w;
    if (max_h > 0 && h * k > max_h) k = (float)max_h / h;
    int tw = (int)lroundf(w * k), th = (int)lroundf(h * k);
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    uint32_t *o = malloc((size_t)tw * th * 4);
    if (!o) { stbi_image_free(d); return 0; }
    for (int y = 0; y < th; y++) {
        int y0 = (int)((long)y * h / th), y1 = (int)((long)(y + 1) * h / th);
        if (y1 <= y0) y1 = y0 + 1;
        for (int x = 0; x < tw; x++) {
            int x0 = (int)((long)x * w / tw), x1 = (int)((long)(x + 1) * w / tw);
            if (x1 <= x0) x1 = x0 + 1;
            unsigned r = 0, g = 0, b = 0, a = 0, cnt = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    const unsigned char *p = d + ((size_t)yy * w + xx) * 4;
                    r += p[0]; g += p[1]; b += p[2]; a += p[3]; cnt++;
                }
            o[(size_t)y * tw + x] = ((a / cnt) << 24) | ((r / cnt) << 16) | ((g / cnt) << 8) | (b / cnt);
        }
    }
    stbi_image_free(d);
    img->px = o;
    img->w = tw;
    img->h = th;
    return 1;
}

static int ends_with(const char *s, const char *e)
{
    size_t a = strlen(s), b = strlen(e);
    return a >= b && strcasecmp(s + a - b, e) == 0;
}

/* the banner: its offset at 0x68 of the header; the icon 4 bits a pixel in 8x8 tiles at 0x20, its 16 colours
   (BGR555, the first see-through) at 0x220, the titles (UTF-16, 128 characters) from 0x240 */
static int banner(const char *rom, unsigned char *b, size_t want)
{
    if (!(ends_with(rom, ".nds") || ends_with(rom, ".dsi"))) return 0;
    FILE *f = fopen(rom, "rb");
    if (!f) return 0;
    unsigned char h[4];
    int ok = 0;
    if (fseek(f, 0x68, SEEK_SET) == 0 && fread(h, 1, 4, f) == 4) {
        unsigned long off = h[0] | h[1] << 8 | h[2] << 16 | (unsigned long)h[3] << 24;
        if (off >= 0x200 && fseek(f, (long)off, SEEK_SET) == 0 && fread(b, 1, want, f) == want) ok = 1;
    }
    fclose(f);
    return ok;
}

int px_nds_icon(const char *rom, PxImage *img)
{
    memset(img, 0, sizeof *img);
    unsigned char b[0x240];
    if (!banner(rom, b, sizeof b)) return 0;
    uint32_t pal[16];
    int any = 0;
    for (int i = 0; i < 16; i++) {
        unsigned v = b[0x220 + i * 2] | b[0x221 + i * 2] << 8;
        unsigned r = v & 31, g = (v >> 5) & 31, bl = (v >> 10) & 31;
        pal[i] = (i ? 0xff000000u : 0) | (r * 255 / 31) << 16 | (g * 255 / 31) << 8 | (bl * 255 / 31);
    }
    uint32_t *o = malloc(32 * 32 * 4);
    if (!o) return 0;
    for (int t = 0; t < 16; t++)
        for (int py = 0; py < 8; py++)
            for (int px = 0; px < 8; px++) {
                unsigned char v = b[0x20 + t * 32 + py * 4 + px / 2];
                int ci = (px & 1) ? v >> 4 : v & 15;
                if (ci) any = 1;
                o[((t / 4) * 8 + py) * 32 + (t % 4) * 8 + px] = pal[ci];
            }
    if (!any) { free(o); return 0; }
    img->px = o;
    img->w = img->h = 32;
    img->pixel_art = 1;
    return 1;
}

static void utf8_put(char **p, char *end, unsigned cp)
{
    char t[4];
    int n;
    if (cp < 0x80) { t[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { t[0] = (char)(0xc0 | cp >> 6); t[1] = (char)(0x80 | (cp & 63)); n = 2; }
    else { t[0] = (char)(0xe0 | cp >> 12); t[1] = (char)(0x80 | ((cp >> 6) & 63)); t[2] = (char)(0x80 | (cp & 63)); n = 3; }
    if (*p + n >= end) return;
    memcpy(*p, t, n);
    *p += n;
}

int px_nds_title(const char *rom, const char *lang, char *out, int outsz)
{
    static unsigned char b[0x840];
    if (outsz < 2 || !banner(rom, b, sizeof b)) return 0;
    /* 0 Japanese, 1 English, 2 French, 3 German, 4 Italian, 5 Spanish */
    int li = 1;
    if (lang) {
        if (!strncmp(lang, "es", 2)) li = 5;
        else if (!strncmp(lang, "fr", 2)) li = 2;
        else if (!strncmp(lang, "de", 2)) li = 3;
        else if (!strncmp(lang, "it", 2)) li = 4;
    }
    for (int pass = 0; pass < 2; pass++, li = 1) {
        const unsigned char *s = b + 0x240 + li * 0x100;
        char *p = out, *end = out + outsz;
        int lines = 0;
        for (int i = 0; i < 128; i++) {
            unsigned cp = s[i * 2] | s[i * 2 + 1] << 8;
            if (!cp) break;
            if (cp == '\n') {
                lines++;
                /* the last line is the publisher: left out */
                int more = 0;
                for (int j = i + 1; j < 128; j++) { unsigned c2 = s[j * 2] | s[j * 2 + 1] << 8; if (!c2) break; if (c2 == '\n') { more = 1; break; } }
                if (!more) break;
                utf8_put(&p, end, ' '); utf8_put(&p, end, '-'); utf8_put(&p, end, ' ');
                continue;
            }
            if (cp < 0x20 || (cp >= 0xd800 && cp < 0xe000)) continue;
            utf8_put(&p, end, cp);
        }
        *p = 0;
        (void)lines;
        while (p > out && p[-1] == ' ') *--p = 0;
        if (*out) return 1;
    }
    return 0;
}

void px_image_draw(PxCanvas *c, float x, float y, float w, float h, const PxImage *img)
{
    if (!img || !img->px || img->w <= 0 || img->h <= 0) return;
    int bx = px_u(c, x), by = px_u(c, y), bw = px_u(c, w), bh = px_u(c, h);
    float k = fminf((float)bw / img->w, (float)bh / img->h);
    if (img->pixel_art) { if (k >= 1) k = floorf(k); }
    else if (k > 1 && k < 1.08f) k = 1;     /* (a cover read for this box: drawn as it is) */
    int dw = (int)lroundf(img->w * k), dh = (int)lroundf(img->h * k);
    if (dw < 1 || dh < 1) return;
    int ox = bx + (bw - dw) / 2, oy = by + (bh - dh) / 2;
    for (int yy = 0; yy < dh; yy++) {
        int ty = oy + yy;
        if (ty < 0 || ty >= c->h) continue;
        int sy = (int)((long)yy * img->h / dh);
        for (int xx = 0; xx < dw; xx++) {
            int tx = ox + xx;
            if (tx < 0 || tx >= c->w) continue;
            uint32_t s = img->px[(size_t)sy * img->w + (size_t)xx * img->w / dw];
            unsigned a = s >> 24;
            if (!a) continue;
            uint32_t *d = &c->px[(size_t)ty * c->w + tx];
            if (a == 255) { *d = s & 0xffffff; continue; }
            unsigned dr = (*d >> 16) & 255, dg = (*d >> 8) & 255, db = *d & 255;
            unsigned sr = (s >> 16) & 255, sg = (s >> 8) & 255, sb = s & 255;
            *d = ((sr * a + dr * (255 - a)) / 255) << 16 | ((sg * a + dg * (255 - a)) / 255) << 8 | ((sb * a + db * (255 - a)) / 255);
        }
    }
}

int px_find_box(const char *rom, char *out, int outsz)
{
    const char *slash = strrchr(rom, '/');
    if (!slash) return 0;
    char dir[1024], stem[512];
    snprintf(dir, sizeof dir, "%.*s/media/box", (int)(slash - rom), rom);
    snprintf(stem, sizeof stem, "%s", slash + 1);
    char *dot = strrchr(stem, '.');
    if (dot) *dot = 0;
    size_t sl = strlen(stem);
    DIR *d = opendir(dir);
    if (!d) return 0;
    char best[512] = "";
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strncasecmp(e->d_name, stem, sl) != 0) continue;
        const char *ext = e->d_name + sl;
        if (!strcasecmp(ext, ".png")) { snprintf(best, sizeof best, "%s", e->d_name); break; }   /* .png first, as banana-ui */
        if (!strcasecmp(ext, ".jpg") && !*best) snprintf(best, sizeof best, "%s", e->d_name);
    }
    closedir(d);
    if (!*best) return 0;
    snprintf(out, outsz, "%s/%s", dir, best);
    return 1;
}

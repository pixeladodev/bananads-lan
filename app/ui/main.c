/* BananaDS LAN: On/Off on both panels. Uninstall turns it off and removes the app.
   Colours are BananaOS's palette (the same values BananaDS's menu uses).
   The typeface is the system's Pixelify Sans. libdrm, both DSI panels. */
#include "stb_truetype.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define C_BG    0x191820
#define C_PANEL 0x211f26
#define C_BLUE  0xf0b429
#define C_LIGHT 0xf1ede2
#define C_GREY  0xa39d8e
#define C_ONSEL 0x17161c
#define C_RED   0xff8a96
#define C_DIM   0x6b6559

enum { ACT_NONE, ACT_TOGGLE, ACT_QUIT, ACT_UNINSTALL };

static volatile sig_atomic_t g_stop;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

typedef struct {
    uint32_t conn, crtc, fb, handle, pitch, size;
    uint8_t *map;
    drmModeModeInfo mode;
} Panel;

static int g_fd = -1, g_w, g_h;
static Panel g_p[2];

static unsigned char *g_ttf;
static unsigned char g_atlas[512 * 512];
static stbtt_bakedchar g_glyph[96];
static int g_px, g_font_ok;

static char g_app[512];

static void px(Panel *p, int x, int y, uint32_t rgb)
{
    uint32_t *row;
    if (x < 0 || y < 0 || x >= g_w || y >= g_h) return;
    row = (uint32_t *)(p->map + (size_t)y * p->pitch);
    row[x] = rgb;
}

static void blend(Panel *p, int x, int y, uint32_t rgb, int a)
{
    uint32_t d, *row;
    int dr, dg, db, sr, sg, sb;
    if (a <= 0 || x < 0 || y < 0 || x >= g_w || y >= g_h) return;
    row = (uint32_t *)(p->map + (size_t)y * p->pitch);
    if (a >= 255) { row[x] = rgb; return; }
    d = row[x];
    dr = (d >> 16) & 255; dg = (d >> 8) & 255; db = d & 255;
    sr = (rgb >> 16) & 255; sg = (rgb >> 8) & 255; sb = rgb & 255;
    row[x] = ((dr + (sr - dr) * a / 255) << 16) | ((dg + (sg - dg) * a / 255) << 8) | (db + (sb - db) * a / 255);
}

static void fill(Panel *p, int x0, int y0, int x1, int y1, uint32_t rgb)
{
    int x, y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > g_w) x1 = g_w;
    if (y1 > g_h) y1 = g_h;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            px(p, x, y, rgb);
}

static int load_font(void)
{
    static const char *const paths[] = {
        "/usr/share/banana/rnds/fonts/PixelifySans-Medium.ttf",
        "/usr/share/banana/rnds/fonts/PixelifySans-Regular.ttf",
        NULL
    };
    FILE *f;
    long n;
    int i;
    for (i = 0; paths[i]; i++) {
        f = fopen(paths[i], "rb");
        if (!f) continue;
        if (fseek(f, 0, SEEK_END) != 0) { fclose(f); continue; }
        n = ftell(f);
        if (n < 1 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); continue; }
        g_ttf = malloc((size_t)n);
        if (!g_ttf || fread(g_ttf, 1, (size_t)n, f) != (size_t)n) {
            free(g_ttf); g_ttf = NULL; fclose(f); continue;
        }
        fclose(f);
        g_px = g_h / 22;
        if (g_px < 16) g_px = 16;
        if (g_px > 42) g_px = 42;
        if (stbtt_BakeFontBitmap(g_ttf, 0, (float)g_px, g_atlas, 512, 512, 32, 96, g_glyph) <= 0)
            continue;
        g_font_ok = 1;
        return 1;
    }
    return 0;
}

static int text_width(const char *s)
{
    float x = 0, y = 0;
    stbtt_aligned_quad q;
    for (; *s; s++) {
        if ((unsigned char)*s < 32 || (unsigned char)*s > 126) { x += g_px / 2.0f; continue; }
        stbtt_GetBakedQuad(g_glyph, 512, 512, (unsigned char)*s - 32, &x, &y, &q, 1);
    }
    return (int)(x + 0.5f);
}

static void text(Panel *p, int x, int y, const char *s, uint32_t rgb)
{
    float xp = (float)x, yp = (float)(y + g_px);
    stbtt_aligned_quad q;
    int ix, iy;
    if (!g_font_ok) return;
    for (; *s; s++) {
        int sx, sy, ax, ay;
        if ((unsigned char)*s < 32 || (unsigned char)*s > 126) { xp += g_px / 2.0f; continue; }
        stbtt_GetBakedQuad(g_glyph, 512, 512, (unsigned char)*s - 32, &xp, &yp, &q, 1);
        sx = (int)(q.x1 - q.x0); sy = (int)(q.y1 - q.y0);
        if (sx < 1 || sy < 1) continue;
        for (iy = 0; iy < sy; iy++) {
            int ty = (int)((q.t0 * 512) + (q.t1 - q.t0) * 512 * iy / sy);
            if (ty < 0) ty = 0;
            if (ty > 511) ty = 511;
            for (ix = 0; ix < sx; ix++) {
                int tx = (int)((q.s0 * 512) + (q.s1 - q.s0) * 512 * ix / sx);
                if (tx < 0) tx = 0;
                if (tx > 511) tx = 511;
                ax = (int)q.x0 + ix;
                ay = (int)q.y0 + iy;
                blend(p, ax, ay, rgb, g_atlas[ty * 512 + tx]);
            }
        }
    }
}

static void planes_upright(void)
{
    drmModePlaneRes *pr;
    uint32_t j;
    drmSetClientCap(g_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    pr = drmModeGetPlaneResources(g_fd);
    if (!pr) return;
    for (j = 0; j < pr->count_planes; j++) {
        drmModeObjectProperties *pp = drmModeObjectGetProperties(g_fd, pr->planes[j], DRM_MODE_OBJECT_PLANE);
        uint32_t i;
        if (!pp) continue;
        for (i = 0; i < pp->count_props; i++) {
            drmModePropertyRes *prop = drmModeGetProperty(g_fd, pp->props[i]);
            if (!prop) continue;
            if (!strcmp(prop->name, "rotation") && pp->prop_values[i] != DRM_MODE_ROTATE_0)
                drmModeObjectSetProperty(g_fd, pr->planes[j], DRM_MODE_OBJECT_PLANE, prop->prop_id, DRM_MODE_ROTATE_0);
            drmModeFreeProperty(prop);
        }
        drmModeFreeObjectProperties(pp);
    }
    drmModeFreePlaneResources(pr);
}

static void panel_free(Panel *p)
{
    if (p->map && p->map != MAP_FAILED) munmap(p->map, p->size);
    if (p->fb) drmModeRmFB(g_fd, p->fb);
    if (p->handle) {
        struct drm_mode_destroy_dumb d;
        memset(&d, 0, sizeof d);
        d.handle = p->handle;
        drmIoctl(g_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
    }
    memset(p, 0, sizeof *p);
}

static void display_close(void)
{
    int s;
    if (g_fd < 0) return;
    drmDropMaster(g_fd);
    for (s = 0; s < 2; s++) panel_free(&g_p[s]);
    close(g_fd);
    g_fd = -1;
}

static int make_buf(Panel *p)
{
    struct drm_mode_create_dumb c;
    struct drm_mode_map_dumb m;
    uint32_t handles[4], pitches[4], offsets[4];
    memset(&c, 0, sizeof c);
    c.width = (uint32_t)g_w;
    c.height = (uint32_t)g_h;
    c.bpp = 32;
    if (drmIoctl(g_fd, DRM_IOCTL_MODE_CREATE_DUMB, &c)) return 0;
    p->handle = c.handle;
    p->pitch = c.pitch;
    p->size = c.size;
    handles[0] = p->handle; pitches[0] = p->pitch; offsets[0] = 0;
    handles[1] = handles[2] = handles[3] = 0;
    pitches[1] = pitches[2] = pitches[3] = 0;
    offsets[1] = offsets[2] = offsets[3] = 0;
    if (drmModeAddFB2(g_fd, (uint32_t)g_w, (uint32_t)g_h, DRM_FORMAT_XRGB8888, handles, pitches, offsets, &p->fb, 0))
        return 0;
    memset(&m, 0, sizeof m);
    m.handle = p->handle;
    if (drmIoctl(g_fd, DRM_IOCTL_MODE_MAP_DUMB, &m)) return 0;
    p->map = mmap(NULL, p->size, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, (off_t)m.offset);
    if (p->map == MAP_FAILED) return 0;
    memset(p->map, 0, p->size);
    return 1;
}

static int display_open(void)
{
    const char *top = getenv("BANANA_TOP");
        char path[288];
    DIR *d;
    struct dirent *e;
    int t;
    if (!top || !top[0]) top = "DSI-2";
    d = opendir("/dev/dri");
    if (!d) { fprintf(stderr, "bananads-lan: no /dev/dri (%s)\n", strerror(errno)); return 0; }
    while ((e = readdir(d))) {
        drmModeRes *res;
        typedef struct { uint32_t conn, crtc; drmModeModeInfo mode; int istop; } Found;
        Found found[2], tmp;
        int nfound = 0;
        uint32_t used[2];
        int i;
        if (strncmp(e->d_name, "card", 4)) continue;
        snprintf(path, sizeof path, "/dev/dri/%s", e->d_name);
        g_fd = open(path, O_RDWR | O_CLOEXEC);
        if (g_fd < 0) continue;
        res = drmModeGetResources(g_fd);
        if (!res) { close(g_fd); g_fd = -1; continue; }
        for (i = 0; i < res->count_connectors && nfound < 2; i++) {
            drmModeConnector *c = drmModeGetConnector(g_fd, res->connectors[i]);
            const char *tn;
            char name[32];
            drmModeModeInfo mode;
            uint32_t crtc = 0;
            int m, en;
            if (!c) continue;
            tn = drmModeGetConnectorTypeName(c->connector_type);
            if (!tn || strcmp(tn, "DSI") || c->connection != DRM_MODE_CONNECTED || c->count_modes < 1) {
                drmModeFreeConnector(c);
                continue;
            }
            mode = c->modes[0];
            for (m = 0; m < c->count_modes; m++)
                if (c->modes[m].type & DRM_MODE_TYPE_PREFERRED) { mode = c->modes[m]; break; }
            for (en = 0; en < c->count_encoders && !crtc; en++) {
                drmModeEncoder *enc = drmModeGetEncoder(g_fd, c->encoders[en]);
                int k;
                if (!enc) continue;
                for (k = 0; k < res->count_crtcs && !crtc; k++) {
                    int u, taken = 0;
                    if (!(enc->possible_crtcs & (1u << k))) continue;
                    for (u = 0; u < nfound; u++) if (used[u] == res->crtcs[k]) taken = 1;
                    if (!taken) crtc = res->crtcs[k];
                }
                drmModeFreeEncoder(enc);
            }
            if (crtc) {
                snprintf(name, sizeof name, "%s-%u", tn, c->connector_type_id);
                used[nfound] = crtc;
                found[nfound].conn = c->connector_id;
                found[nfound].crtc = crtc;
                found[nfound].mode = mode;
                found[nfound].istop = !strcmp(name, top);
                nfound++;
            }
            drmModeFreeConnector(c);
        }
        drmModeFreeResources(res);
        if (nfound < 2) { close(g_fd); g_fd = -1; continue; }
        if (!found[0].istop && !found[1].istop) {
            tmp = found[0];
            found[0] = found[1];
            found[1] = tmp;
        }
        if (found[1].istop) {
            tmp = found[0];
            found[0] = found[1];
            found[1] = tmp;
        }
        if (found[0].mode.hdisplay != found[1].mode.hdisplay || found[0].mode.vdisplay != found[1].mode.vdisplay) {
            fprintf(stderr, "bananads-lan: the two panels differ in size\n");
            close(g_fd); g_fd = -1; closedir(d); return 0;
        }
        for (t = 0; t < 50; t++) { if (drmSetMaster(g_fd) == 0) break; usleep(20000); }
        g_w = found[0].mode.hdisplay;
        g_h = found[0].mode.vdisplay;
        for (i = 0; i < 2; i++) {
            memset(&g_p[i], 0, sizeof g_p[i]);
            g_p[i].conn = found[i].conn;
            g_p[i].crtc = found[i].crtc;
            g_p[i].mode = found[i].mode;
            if (!make_buf(&g_p[i])) {
                fprintf(stderr, "bananads-lan: no scanout buffer (%s)\n", strerror(errno));
                display_close();
                closedir(d);
                return 0;
            }
        }
        planes_upright();
        closedir(d);
        return 1;
    }
    closedir(d);
    fprintf(stderr, "bananads-lan: no two DSI panels\n");
    return 0;
}

static void present(void)
{
    int s;
    for (s = 0; s < 2; s++)
        drmModeSetCrtc(g_fd, g_p[s].crtc, g_p[s].fb, 0, 0, &g_p[s].conn, 1, &g_p[s].mode);
}

typedef struct {
    int supported, enabled, mounted;
    char reason[240];
    char emulator[64];
    char os[32];
    char version[32];
} State;

static void copy_str(char *dst, size_t n, const char *src)
{
    size_t i = 0;
    if (n == 0) return;
    while (i + 1 < n && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void kv(char *line, const char *key, char *dst, int n)
{
    size_t k = strlen(key);
    if (n > 0 && strncmp(line, key, k) == 0 && line[k] == '=')
        copy_str(dst, (size_t)n, line + k + 1);
}

static int load_state(State *st)
{
    char cmd[640], line[512];
    FILE *f;
    memset(st, 0, sizeof *st);
    snprintf(st->emulator, sizeof st->emulator, "unset");
    snprintf(cmd, sizeof cmd, "sh '%s/status.sh' 2>/dev/null", g_app);
    f = popen(cmd, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        if (!strncmp(line, "supported=", 10)) st->supported = !strcmp(line + 10, "yes");
        else if (!strncmp(line, "enabled=", 8)) st->enabled = !strcmp(line + 8, "yes");
        else if (!strncmp(line, "mounted=", 8)) st->mounted = !strcmp(line + 8, "yes");
        else if (!strncmp(line, "reason=", 7)) kv(line, "reason", st->reason, (int)sizeof st->reason);
        else if (!strncmp(line, "emulator=", 9)) copy_str(st->emulator, sizeof st->emulator, line + 9);
        else if (!strncmp(line, "os=", 3)) copy_str(st->os, sizeof st->os, line + 3);
        else if (!strncmp(line, "version=", 8)) copy_str(st->version, sizeof st->version, line + 8);
    }
    pclose(f);
    return 1;
}

static int run_sh(const char *script, char *err, int errn)
{
    char cmd[640], line[512];
    FILE *f;
    int st;
    err[0] = 0;
    snprintf(cmd, sizeof cmd, "sh '%s/%s' 2>&1", g_app, script);
    f = popen(cmd, "r");
    if (!f) { snprintf(err, (size_t)errn, "could not run %s", script); return 0; }
    if (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        copy_str(err, (size_t)errn, line);
    }
    st = pclose(f);
    if (st == 0) err[0] = 0;
    return st == 0;
}

static void draw(const State *st, const char *err, int confirm)
{
    int s, margin, row_y, row_h, y, x;
    const char *val = st->enabled ? "On" : "Off";
    char line[160];
    for (s = 0; s < 2; s++) {
        Panel *p = &g_p[s];
        fill(p, 0, 0, g_w, g_h, C_BG);
        margin = g_w / 18;
        if (confirm == 2) {
            text(p, margin, g_h / 6, "One folder too deep.", C_LIGHT);
            text(p, margin, g_h / 6 + g_px * 2, "bananads-lan/bananads-lan", C_GREY);
            text(p, margin, g_h / 6 + g_px * 4, "A moves it up one folder.", C_GREY);
            text(p, margin, g_h - g_px * 2, "A  move it up    B  back", C_GREY);
            continue;
        }
        if (confirm == 3) {
            text(p, margin, g_h / 5, "Moved.", C_LIGHT);
            text(p, margin, g_h / 5 + g_px * 2, "Open BananaDS LAN again.", C_GREY);
            text(p, margin, g_h - g_px * 2, "A  or B  back", C_GREY);
            continue;
        }
        if (confirm) {
            text(p, margin, g_h / 5, "Uninstall BananaDS LAN?", C_LIGHT);
            text(p, margin, g_h / 5 + g_px * 2, "Turns it off and deletes this app.", C_GREY);
            text(p, margin, g_h - g_px * 2, "A  uninstall      B  cancel", C_GREY);
            continue;
        }
        if (s == 0) {
            text(p, margin, g_h / 7, "BananaDS LAN", C_LIGHT);
            row_y = g_h / 2 - g_px;
            row_h = g_px * 2 + g_px / 2;
            fill(p, margin, row_y, g_w - margin, row_y + row_h, C_BLUE);
            text(p, margin + g_px, row_y + g_px / 3, "Local multiplayer", C_ONSEL);
            x = g_w - margin - g_px - text_width(val);
            text(p, x, row_y + g_px / 3, val, C_ONSEL);
            text(p, margin, g_h - g_px * 2, "A  toggle    Y  uninstall    B  back", C_GREY);
        } else {
            text(p, margin, g_h / 10, "Status", C_LIGHT);
            y = g_h / 10 + g_px * 2;
            snprintf(line, sizeof line, "BananaOS %s", st->os[0] ? st->os : "?");
            text(p, margin, y, line, C_LIGHT);
            text(p, margin, y + g_px + 8, st->supported ? "Supported" : "Not supported", st->supported ? C_LIGHT : C_RED);
            y += (g_px + 8) * 2 + g_px / 2;
            snprintf(line, sizeof line, "Mounted  %s", st->mounted ? "yes" : "no");
            text(p, margin, y, line, C_GREY);
            y += g_px + 10;
            if (!strcmp(st->emulator, "bananads"))
                snprintf(line, sizeof line, "Emulator  BananaDS");
            else
                snprintf(line, sizeof line, "Emulator  not BananaDS");
            text(p, margin, y, line, !strcmp(st->emulator, "bananads") ? C_GREY : C_RED);
            y += g_px + 10;
            text(p, margin, y, "Settings > Nintendo DS > Emulator", C_DIM);
            y += g_px * 2;
            snprintf(line, sizeof line, "App %s", st->version[0] ? st->version : "?");
            text(p, margin, y, line, C_DIM);
            if (err && err[0])
                text(p, margin, y + g_px * 2, err, C_RED);
            else if (!st->supported && st->reason[0])
                text(p, margin, y + g_px * 2, st->reason, C_RED);
        }
    }
    present();
}

static int key_action(int code, int value)
{
    if (value != 1) return ACT_NONE;
    switch (code) {
    case BTN_EAST: case KEY_ENTER: return ACT_TOGGLE;
    case BTN_WEST: return ACT_UNINSTALL; /* Y, beside B */
    case BTN_SOUTH: case KEY_BACKSPACE: case KEY_ESC: return ACT_QUIT;
    default: return ACT_NONE;
    }
}

static int app_is_nested(void)
{
    const char *tail = "/bananads-lan/bananads-lan";
    size_t n = strlen(g_app), t = strlen(tail);
    if (n < t) return 0;
    return strcmp(g_app + n - t, tail) == 0;
}

static int app_dir(void)
{
    char exe[512];
    ssize_t n;
    char *slash;
    n = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (n < 0) return 0;
    exe[n] = 0;
    slash = strrchr(exe, '/');
    if (!slash) return 0;
    *slash = 0; /* .../ui */
    slash = strrchr(exe, '/');
    if (!slash) return 0;
    *slash = 0;
    snprintf(g_app, sizeof g_app, "%s", exe);
    return 1;
}

int main(void)
{
    State st;
    char err[240];
    int confirm = 0;
    struct pollfd pf[32];
    int nf = 0, i;
    DIR *d;
    struct dirent *e;
    err[0] = 0;
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    if (!app_dir()) { fprintf(stderr, "bananads-lan: cannot find the app directory\n"); return 1; }
    if (app_is_nested()) confirm = 2;
    if (!display_open()) return 1;
    if (!load_font()) fprintf(stderr, "bananads-lan: Pixelify Sans was not found\n");
    d = opendir("/dev/input");
    if (d) {
        while ((e = readdir(d)) && nf < 32) {
            char path[320];
            int fd;
            if (strncmp(e->d_name, "event", 5)) continue;
            snprintf(path, sizeof path, "/dev/input/%s", e->d_name);
            fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
            if (fd < 0) continue;
            pf[nf].fd = fd;
            pf[nf].events = POLLIN;
            nf++;
        }
        closedir(d);
    }
    load_state(&st);
    draw(&st, err, confirm);
    while (!g_stop) {
        int r = poll(pf, (nfds_t)nf, 200);
        int action = ACT_NONE;
        if (r < 0) { if (errno == EINTR) continue; break; }
        for (i = 0; i < nf; i++) {
            struct input_event ev;
            if (!(pf[i].revents & POLLIN)) continue;
            while (read(pf[i].fd, &ev, sizeof ev) == (ssize_t)sizeof ev) {
                int a;
                if (ev.type != EV_KEY) continue;
                a = key_action(ev.code, ev.value);
                if (a) action = a;
            }
        }
        if (action == ACT_QUIT) {
            if (confirm == 2 || confirm == 3) break;
            if (!confirm) break;
            confirm = 0;
            err[0] = 0;
            draw(&st, err, confirm);
        } else if (action == ACT_TOGGLE && confirm == 3) {
            break;
        } else if (action == ACT_TOGGLE && confirm == 2) {
            if (run_sh("fix-nest.sh", err, (int)sizeof err)) confirm = 3;
            draw(&st, err, confirm);
        } else if (action == ACT_UNINSTALL && !confirm) {
            confirm = 1;
            draw(&st, err, confirm);
        } else if (action == ACT_TOGGLE && confirm) {
            if (run_sh("uninstall.sh", err, (int)sizeof err)) break;
            confirm = 0;
            load_state(&st);
            draw(&st, err, confirm);
        } else if (action == ACT_TOGGLE) {
            run_sh(st.enabled ? "revert.sh" : "apply.sh", err, (int)sizeof err);
            load_state(&st);
            draw(&st, err, confirm);
        }
    }
    for (i = 0; i < nf; i++) close(pf[i].fd);
    display_close();
    free(g_ttf);
    return 0;
}

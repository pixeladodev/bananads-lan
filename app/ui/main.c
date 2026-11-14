/* BananaDS LAN: On/Off on both panels, and its own updates (update.py, through updater.c). Uninstall turns it off
   and removes the app.
   Drawn in the menu's look by view.c and pixel.c (the theme and accent from settings.conf, Pixelify Sans).
   libdrm, both DSI panels; the buttons, and touch on the bottom panel. */
#include "view.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <linux/input.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

static volatile sig_atomic_t g_stop;

static void on_signal(int sig) { (void)sig; g_stop = 1; }

typedef struct {
    uint32_t conn, crtc, fb, handle, pitch, size;
    uint8_t *map;
    drmModeModeInfo mode;
} Panel;

static int g_fd = -1, g_w, g_h;
static Panel g_p[2];
static uint32_t *g_buf[2];

static char g_app[512];
static int g_exit;                /* UPD_RESTART after an update: tile.sh opens the new version */
/* a game picked in the list: its path in PLAY_FILE, and tile.sh runs it on the app's BananaDS (run-emu) */
#define PLAY_EXIT 76
#define PLAY_FILE "/tmp/bananads-lan.play"
#define LAST_FILE "/tmp/bananads-lan.last"

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

/* a drawn canvas into its panel's scanout buffer */
static void blit(int s)
{
    int y;
    for (y = 0; y < g_h; y++)
        memcpy(g_p[s].map + (size_t)y * g_p[s].pitch, g_buf[s] + (size_t)y * (size_t)g_w, (size_t)g_w * 4);
}

static void copy_str(char *dst, size_t n, const char *src)
{
    size_t i = 0;
    if (n == 0) return;
    while (i + 1 < n && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static long ticks(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static void manifest(const char *key, char *dst, size_t n)
{
    char path[640], line[256];
    size_t k = strlen(key);
    FILE *f;
    dst[0] = 0;
    snprintf(path, sizeof path, "%s/manifest", g_app);
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, key, k) && line[k] == '=') {
            char *e = line + strlen(line);
            while (e > line && (e[-1] == '\n' || e[-1] == '\r')) *--e = 0;
            copy_str(dst, n, line + k + 1);
            break;
        }
    }
    fclose(f);
}

/* wlan0's IPv4 address (the home network or Direct Link's 10.42.0.x) */
static void wlan_ip(char *dst, size_t n)
{
    struct ifaddrs *ifa, *i;
    dst[0] = 0;
    if (getifaddrs(&ifa) != 0) return;
    for (i = ifa; i; i = i->ifa_next)
        if (i->ifa_addr && i->ifa_addr->sa_family == AF_INET && !strcmp(i->ifa_name, "wlan0")) {
            inet_ntop(AF_INET, &((struct sockaddr_in *)i->ifa_addr)->sin_addr, dst, (socklen_t)n);
            break;
        }
    freeifaddrs(ifa);
}

static int load_state(Model *m)
{
    char cmd[640], line[512];
    FILE *f;
    m->supported = m->enabled = m->mounted = 0;
    m->reason[0] = m->os[0] = m->version[0] = m->model[0] = 0;
    snprintf(m->emulator, sizeof m->emulator, "unset");
    manifest("bananaos", m->want_os, sizeof m->want_os);
    wlan_ip(m->ip, sizeof m->ip);
    snprintf(cmd, sizeof cmd, "sh '%s/status.sh' 2>/dev/null", g_app);
    f = popen(cmd, "r");
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        if (!strncmp(line, "supported=", 10)) m->supported = !strcmp(line + 10, "yes");
        else if (!strncmp(line, "enabled=", 8)) m->enabled = !strcmp(line + 8, "yes");
        else if (!strncmp(line, "mounted=", 8)) m->mounted = !strcmp(line + 8, "yes");
        else if (!strncmp(line, "reason=", 7)) copy_str(m->reason, sizeof m->reason, line + 7);
        else if (!strncmp(line, "emulator=", 9)) copy_str(m->emulator, sizeof m->emulator, line + 9);
        else if (!strncmp(line, "os=", 3)) copy_str(m->os, sizeof m->os, line + 3);
        else if (!strncmp(line, "version=", 8)) copy_str(m->version, sizeof m->version, line + 8);
        else if (!strncmp(line, "model=", 6)) copy_str(m->model, sizeof m->model, line + 6);
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

static PxSettings g_settings;

/* the status row's battery and clock; 1 when they changed */
static int status_tick(Model *m)
{
    char clock[16];
    int ch = 0, bat = px_battery(&ch), changed;
    px_clock(clock, sizeof clock, &g_settings);
    changed = bat != m->battery || ch != m->charging || strcmp(clock, m->clock);
    m->battery = bat;
    m->charging = ch;
    copy_str(m->clock, sizeof m->clock, clock);
    return changed;
}

static void draw(const Model *m)
{
    PxCanvas c;
    px_canvas(&c, g_buf[0], g_w, g_h);
    view_top(&c, m);
    blit(0);
    px_canvas(&c, g_buf[1], g_w, g_h);
    view_bottom(&c, m);
    blit(1);
    present();
}

enum { ACT_NONE, ACT_UP, ACT_DOWN, ACT_LEFT, ACT_RIGHT, ACT_OK, ACT_BACK, ACT_Y, ACT_QUIT, ACT_ROW, ACT_ABOUT };

/* Nintendo layout: A is the east button, B the south one, Y the west one */
static int key_action(int code, int value)
{
    /* held: the list keeps moving (the kernel's autorepeat), nothing else repeats */
    if (value == 2 && (code == KEY_UP || code == BTN_DPAD_UP || code == KEY_DOWN || code == BTN_DPAD_DOWN))
        value = 1;
    if (value != 1) return ACT_NONE;
    switch (code) {
    case KEY_UP: case BTN_DPAD_UP: return ACT_UP;
    case KEY_DOWN: case BTN_DPAD_DOWN: return ACT_DOWN;
    case KEY_LEFT: case BTN_DPAD_LEFT: return ACT_LEFT;
    case KEY_RIGHT: case BTN_DPAD_RIGHT: return ACT_RIGHT;
    case BTN_EAST: case KEY_ENTER: return ACT_OK;
    case BTN_WEST: return ACT_Y;
    case BTN_SOUTH: case KEY_BACKSPACE: case KEY_ESC: return ACT_BACK;
    case BTN_START: return ACT_QUIT;
    case BTN_SELECT: return ACT_ABOUT;
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

/* ---- the games list ---- */
static int game_ext(const char *name)
{
    const char *dot = strrchr(name, '.');
    char e[8];
    int i;
    if (!dot || strlen(dot + 1) >= sizeof e) return 0;
    for (i = 0; dot[1 + i]; i++) e[i] = (char)tolower((unsigned char)dot[1 + i]);
    e[i] = 0;
    return !strcmp(e, "nds") || !strcmp(e, "dsi") || !strcmp(e, "zip");
}

static const char *base_of(const char *p)
{
    const char *b = strrchr(p, '/');
    return b ? b + 1 : p;
}

static int game_cmp(const void *a, const void *b)
{
    return strcasecmp(base_of((const char *)a), base_of((const char *)b));
}

/* the DS folders the menu's library reads: roms/nds on the card (BANANA_ROMS, as banana-ui takes it), and the
   second card's while it is in (/run/banana/sd2/roms) */
static void scan_games(Model *m)
{
    const char *roms = getenv("BANANA_ROMS");
    char dirs[2][320];
    int d;
    snprintf(dirs[0], sizeof dirs[0], "%s/nds", roms && roms[0] ? roms : "/data/roms");
    snprintf(dirs[1], sizeof dirs[1], "/run/banana/sd2/roms/nds");
    if (!m->games) m->games = calloc(MAX_GAMES, GAME_PATH);
    m->ngames = 0;
    if (!m->games) return;
    for (d = 0; d < 2; d++) {
        DIR *dir = opendir(dirs[d]);
        struct dirent *e;
        if (!dir) continue;
        while ((e = readdir(dir)) && m->ngames < MAX_GAMES) {
            char path[GAME_PATH + 8];
            struct stat st;
            if (e->d_name[0] == '.' || !game_ext(e->d_name)) continue;
            if (snprintf(path, sizeof path, "%s/%s", dirs[d], e->d_name) >= GAME_PATH) continue;
            if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
            copy_str(m->games[m->ngames++], GAME_PATH, path);
        }
        closedir(dir);
    }
    qsort(m->games, (size_t)m->ngames, GAME_PATH, game_cmp);
    if (m->gsel >= m->ngames) m->gsel = m->ngames > 0 ? m->ngames - 1 : 0;
}

/* the picked game's save (saves/nds/<name>.sav, the one both BananaDS share), and the list scrolled to it */
static void game_pick(Model *m, int g)
{
    char name[GAME_PATH], path[GAME_PATH + 64], emu[640];
    struct stat st;
    if (m->ngames <= 0) { m->gsel = m->gtop = 0; m->gsave = -1; return; }
    if (g < 0) g = 0;
    if (g >= m->ngames) g = m->ngames - 1;
    m->gsel = g;
    if (m->gsel < m->gtop) m->gtop = m->gsel;
    if (m->gsel >= m->gtop + GAME_ROWS) m->gtop = m->gsel - GAME_ROWS + 1;
    view_game_name(m->games[g], name, (int)sizeof name);
    snprintf(path, sizeof path, "/data/saves/nds/%s.sav", name);
    m->gsave = stat(path, &st) == 0 ? (long)st.st_size : -1;
    snprintf(emu, sizeof emu, "%s/emu/bananads", g_app);
    m->emu_ok = stat(emu, &st) == 0 && st.st_size > 0;
}

static void games_open(Model *m, const char *last)
{
    int i;
    scan_games(m);
    m->view = V_GAMES;
    if (last && last[0])
        for (i = 0; i < m->ngames; i++)
            if (!strcmp(m->games[i], last)) { m->gsel = i; break; }
    game_pick(m, m->gsel);
}

static int write_line(const char *path, const char *s)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fprintf(f, "%s\n", s);
    return fclose(f) == 0;
}

/* one action; returns 1 to leave */
static int handle(Model *m, int act, int arg)
{
    if (act == ACT_QUIT) return !upd_installing(&m->upd);
    if (m->view == V_NEST) {
        if (act == ACT_OK) {
            if (run_sh("fix-nest.sh", m->err, (int)sizeof m->err)) { m->view = V_NEST_OK; m->err[0] = 0; }
        } else if (act == ACT_BACK) return 1;
        return 0;
    }
    if (m->view == V_NEST_OK) return act == ACT_OK || act == ACT_BACK;
    if (m->view == V_GAMES) {
        switch (act) {
        case ACT_BACK: m->view = V_MAIN; break;
        case ACT_UP: game_pick(m, m->gsel > 0 ? m->gsel - 1 : m->ngames - 1); break;
        case ACT_DOWN: game_pick(m, m->gsel < m->ngames - 1 ? m->gsel + 1 : 0); break;
        case ACT_LEFT: game_pick(m, m->gsel - GAME_ROWS); break;
        case ACT_RIGHT: game_pick(m, m->gsel + GAME_ROWS); break;
        case ACT_ABOUT: m->from_games = 1; m->view = V_ABOUT; break;
        case ACT_ROW:
            if (arg != m->gsel) { game_pick(m, arg); break; }
            /* fall through */
        case ACT_OK:
            if (m->ngames <= 0 || !m->emu_ok) break;
            if (!write_line(PLAY_FILE, m->games[m->gsel])) break;
            write_line(LAST_FILE, m->games[m->gsel]);
            g_exit = PLAY_EXIT;
            return 1;
        default: break;
        }
        return 0;
    }
    if (m->view == V_ABOUT) {
        if (act == ACT_BACK || act == ACT_ABOUT || act == ACT_OK) m->view = m->games && m->from_games ? V_GAMES : V_MAIN;
        return 0;
    }
    if (m->view == V_UPDATED) {
        /* the new version is in place: tile.sh opens it */
        if (act == ACT_OK) { g_exit = UPD_RESTART; return 1; }
        return 0;
    }
    if (m->view == V_UPDATE) {
        if (act == ACT_OK) { m->view = V_MAIN; m->err[0] = 0; upd_start(&m->upd, g_app, "bananads-lan", 1); }
        else if (act == ACT_BACK) m->view = V_MAIN;
        return 0;
    }
    /* while it installs, the app stays open and the list stays as it is */
    if (upd_installing(&m->upd) && act != ACT_UP && act != ACT_DOWN && act != ACT_ABOUT) return 0;
    if (m->view == V_UNINSTALL) {
        if (act == ACT_OK) {
            if (run_sh("uninstall.sh", m->err, (int)sizeof m->err)) return 1;
            m->view = V_MAIN;
            load_state(m);
        } else if (act == ACT_BACK) { m->view = V_MAIN; m->err[0] = 0; }
        return 0;
    }
    switch (act) {
    case ACT_BACK: return 1;
    case ACT_UP: if (m->cursor > 0) m->cursor--; break;
    case ACT_DOWN: if (m->cursor < ROW_COUNT - 1) m->cursor++; break;
    case ACT_Y: m->err[0] = 0; m->view = V_UNINSTALL; break;
    case ACT_ABOUT: m->from_games = 0; m->view = V_ABOUT; break;
    case ACT_ROW:
        /* a tap: the row under the finger, and its action when it already had the cursor */
        if (arg != m->cursor) { m->cursor = arg; break; }
        /* fall through */
    case ACT_OK:
    case ACT_LEFT:
    case ACT_RIGHT:
        if (m->cursor == ROW_PLAY) {
            if (act != ACT_LEFT) games_open(m, NULL);
        } else if (m->cursor == ROW_MP) {
            run_sh(m->enabled ? "revert.sh" : "apply.sh", m->err, (int)sizeof m->err);
            load_state(m);
        } else if (m->cursor == ROW_UPDATES && act != ACT_LEFT && act != ACT_RIGHT && !upd_running(&m->upd)) {
            if (m->upd.state == UPD_AVAILABLE) m->view = V_UPDATE;
            else upd_start(&m->upd, g_app, "bananads-lan", 0);
        }
        break;
    default: break;
    }
    return 0;
}

static int bit_on(int fd, int ev, int code)
{
    unsigned long b[KEY_MAX / (8 * (int)sizeof(long)) + 1];
    memset(b, 0, sizeof b);
    if (ioctl(fd, EVIOCGBIT(ev, sizeof b), b) < 0) return 0;
    return (b[code / (8 * (int)sizeof(long))] >> (code % (8 * (int)sizeof(long)))) & 1;
}

typedef struct {
    int fd, touch;
    int minx, maxx, miny, maxy;
    int x, y, down, was;
} Dev;

static int touch_px(int v, int lo, int hi, int span)
{
    if (hi <= lo || span <= 0) return 0;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (int)((long)(v - lo) * span / (hi - lo + 1));
}

/* the bottom panel's touchscreen is the one on fe5e0000.i2c (banana-ui finds it the same way) */
static int open_inputs(Dev *dev, int max)
{
    DIR *d = opendir("/dev/input");
    struct dirent *e;
    int n = 0;
    if (!d) return 0;
    while ((e = readdir(d)) && n < max) {
        char path[320], sys[320], real[PATH_MAX];
        int fd, bottom = 0;
        if (strncmp(e->d_name, "event", 5)) continue;
        snprintf(path, sizeof path, "/dev/input/%s", e->d_name);
        fd = open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK);
        if (fd < 0) continue;
        snprintf(sys, sizeof sys, "/sys/class/input/%s/device", e->d_name);
        if (realpath(sys, real) && strstr(real, "fe5e0000.i2c")) bottom = 1;
        memset(&dev[n], 0, sizeof dev[n]);
        dev[n].fd = fd;
        if (bottom && (bit_on(fd, EV_ABS, ABS_MT_POSITION_X) || bit_on(fd, EV_ABS, ABS_X))) {
            struct input_absinfo ax, ay;
            int mx = bit_on(fd, EV_ABS, ABS_MT_POSITION_X) ? ABS_MT_POSITION_X : ABS_X;
            int my = bit_on(fd, EV_ABS, ABS_MT_POSITION_Y) ? ABS_MT_POSITION_Y : ABS_Y;
            memset(&ax, 0, sizeof ax);
            memset(&ay, 0, sizeof ay);
            ioctl(fd, EVIOCGABS(mx), &ax);
            ioctl(fd, EVIOCGABS(my), &ay);
            dev[n].touch = 1;
            dev[n].minx = ax.minimum;
            dev[n].maxx = ax.maximum;
            dev[n].miny = ay.minimum;
            dev[n].maxy = ay.maximum;
            dev[n].x = ax.value;
            dev[n].y = ay.value;
        }
        n++;
    }
    closedir(d);
    return n;
}

/* a touch that lifted: what is under it on the bottom panel */
static int touch_action(int hx, int hy, int *arg)
{
    int h, act = ACT_NONE;
    for (h = 0; h < g_nhits; h++) {
        PxRect r = g_hits[h].r;
        if (hx < r.x || hx >= r.x + r.w || hy < r.y || hy >= r.y + r.h) continue;
        switch (g_hits[h].id) {
        case HIT_ROW: act = ACT_ROW; *arg = g_hits[h].arg; break;
        case HIT_BACK: act = ACT_BACK; break;
        case HIT_OK: act = ACT_OK; break;
        case HIT_Y: act = ACT_Y; break;
        case HIT_SELECT: act = ACT_ABOUT; break;
        default: break;
        }
    }
    return act;
}

int main(int argc, char **argv)
{
    Model m;
    Dev dev[32];
    int ndev, i, dirty = 1;
    memset(&m, 0, sizeof m);
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    if (!app_dir()) { fprintf(stderr, "bananads-lan: cannot find the app directory\n"); return 1; }
    if (app_is_nested()) m.view = V_NEST;
    px_settings_read(NULL, &g_settings);
    px_pal_make(&px, &g_settings);
    px_lang_set(g_settings.language, view_tr);
    px_lang_add(upd_tr);
    if (!display_open()) return 1;
    g_buf[0] = calloc((size_t)g_w * (size_t)g_h, sizeof(uint32_t));
    g_buf[1] = calloc((size_t)g_w * (size_t)g_h, sizeof(uint32_t));
    if (!g_buf[0] || !g_buf[1]) { display_close(); return 1; }
    if (!px_fonts_load(NULL)) fprintf(stderr, "bananads-lan: Pixelify Sans was not found\n");
    ndev = open_inputs(dev, 32);
    load_state(&m);
    copy_str(m.upd.current, sizeof m.upd.current, m.version);
    /* a look for a newer version as it opens (in the background: no network just says so) */
    if (m.view == V_MAIN) upd_start(&m.upd, g_app, "bananads-lan", 0);
    /* back from a game (tile.sh --games): the list again, on the game just played */
    if (m.view == V_MAIN && argc > 1 && !strcmp(argv[1], "--games")) {
        char last[GAME_PATH] = "";
        FILE *f = fopen(LAST_FILE, "r");
        if (f) {
            if (fgets(last, sizeof last, f)) last[strcspn(last, "\n")] = 0;
            fclose(f);
        }
        games_open(&m, last);
    } else {
        scan_games(&m);
        game_pick(&m, 0);
    }
    status_tick(&m);
    m.dots = 1;
    while (!g_stop) {
        struct pollfd pf[32];
        int r, act = ACT_NONE, arg = 0, busy = upd_running(&m.upd);
        if (dirty) draw(&m);
        dirty = 0;
        for (i = 0; i < ndev; i++) { pf[i].fd = dev[i].fd; pf[i].events = POLLIN; pf[i].revents = 0; }
        r = poll(pf, (nfds_t)ndev, busy ? 200 : 1000);
        if (r < 0) { if (errno == EINTR) continue; break; }
        if (busy) {
            int d = (int)((ticks() / 400) % 3) + 1;
            if (d != m.dots) { m.dots = d; dirty = 1; }
            if (upd_poll(&m.upd)) {
                dirty = 1;
                if (!upd_running(&m.upd) && m.upd.installing && m.upd.state == UPD_DONE) {
                    m.view = V_UPDATED;
                    load_state(&m);
                }
            }
        }
        if (r == 0) {                            /* a quiet second: only the clock and the battery can change */
            if (status_tick(&m)) dirty = 1;
            continue;
        }
        for (i = 0; i < ndev; i++) {
            struct input_event ev;
            if (!(pf[i].revents & POLLIN)) continue;
            while (read(dev[i].fd, &ev, sizeof ev) == (ssize_t)sizeof ev) {
                if (dev[i].touch) {
                    if (ev.type == EV_ABS && (ev.code == ABS_MT_POSITION_X || ev.code == ABS_X)) dev[i].x = ev.value;
                    else if (ev.type == EV_ABS && (ev.code == ABS_MT_POSITION_Y || ev.code == ABS_Y)) dev[i].y = ev.value;
                    else if (ev.type == EV_ABS && ev.code == ABS_MT_TRACKING_ID) dev[i].down = ev.value >= 0;
                    else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) dev[i].down = ev.value != 0;
                    else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
                        if (!dev[i].down && dev[i].was) {
                            int hx = touch_px(dev[i].x, dev[i].minx, dev[i].maxx, g_w);
                            int hy = touch_px(dev[i].y, dev[i].miny, dev[i].maxy, g_h);
                            int a = touch_action(hx, hy, &arg);
                            if (a) act = a;
                        }
                        dev[i].was = dev[i].down;
                    }
                    continue;
                }
                if (ev.type == EV_KEY) {
                    int a = key_action(ev.code, ev.value);
                    if (a) act = a;
                }
            }
        }
        if (act != ACT_NONE) {
            if (handle(&m, act, arg)) break;
            dirty = 1;
        }
        if (status_tick(&m)) dirty = 1;
    }
    for (i = 0; i < ndev; i++) close(dev[i].fd);
    display_close();
    free(g_buf[0]);
    free(g_buf[1]);
    px_fonts_free();
    free(m.games);
    return g_exit;
}

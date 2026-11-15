/* Preview: BananaDS LAN's panels as PNG files, on a computer (build/preview.sh). Both panels in one picture, top
   over bottom, for each panel size (RG DS 640x480, RG DS Plus 1024x768) and theme. Nothing here runs on a handheld. */
#include "pixel_png.h"
#include "view.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define PREVIEW_THUMB_MAX 128
#define PREVIEW_ROM_MAX 128

static Model base(void)
{
    Model m;
    memset(&m, 0, sizeof m);
    m.enabled = 1;
    snprintf(m.os, sizeof m.os, "0.5.3.4");
    snprintf(m.version, sizeof m.version, "0.4.0");
    snprintf(m.model, sizeof m.model, "rgds_plus");
    snprintf(m.ip, sizeof m.ip, "192.168.18.184");
    m.battery = 82;
    snprintf(m.clock, sizeof m.clock, "14:30");
    m.upd.state = UPD_UPTODATE;
    snprintf(m.upd.current, sizeof m.upd.current, "0.4.0");
    snprintf(m.upd.latest, sizeof m.upd.latest, "0.4.0");
    m.dots = 2;
    return m;
}

/* the library's pictures at a panel's size, as main.c reads them on the handheld (preview-media: a few covers from
   the console, and a ROM's banner for one without) */
static void pictures(Model *m, int h)
{
    int cap = PREVIEW_THUMB_MAX;
    static PxImage thumbs[PREVIEW_THUMB_MAX], art;
    static signed char st[PREVIEW_THUMB_MAX];
    float s = h / 480.0f;
    int i;
    if (!getenv("PREVIEW_README_ONLY")) cap = 64;
    for (i = 0; i < cap; i++) if (st[i] == 1) px_image_free(&thumbs[i]);
    memset(st, 0, (size_t)cap * sizeof *st);
    px_image_free(&art);
    m->thumbs = thumbs;
    m->tstate = st;
    for (i = 0; i < m->ngames && i < cap; i++) {
        int ok = px_nds_icon(m->games[i], &thumbs[i]);
        st[i] = ok ? 1 : -1;
    }
    m->art_game = -1;
    if (m->ngames > 0) {
        char box[GAME_PATH + 64];
        int ok = 0;
        if (px_find_box(m->games[m->gsel], box, (int)sizeof box))
            ok = px_image_load(box, (int)(ART_W * s), (int)(ART_H * s), &art);
        if (!ok) ok = px_nds_icon(m->games[m->gsel], &art);
        m->art = art;
        m->art_game = ok ? m->gsel : -1;
    }
}

static int shot(const char *dir, const char *name, int w, int h, const char *theme, const char *accent,
                const char *lang, const Model *m)
{
    PxSettings st;
    PxCanvas top, bot;
    int gap = h / 24, ok;
    uint32_t *buf = calloc((size_t)w * (size_t)(h * 2 + gap), sizeof *buf);
    char path[512];
    if (!buf) return 0;
    px_settings_read("/nonexistent", &st);
    snprintf(st.theme, sizeof st.theme, "%s", theme);
    snprintf(st.accent, sizeof st.accent, "%s", accent);
    px_pal_make(&px, &st);
    px_lang_set(lang, view_tr);
    px_lang_add(upd_tr);
    px_canvas(&top, buf, w, h);
    px_canvas(&bot, buf + (size_t)w * (size_t)(h + gap), w, h);
    {
        int i;
        for (i = 0; i < w * gap; i++) buf[(size_t)w * (size_t)h + (size_t)i] = 0x000000;
    }
    if (m->view == V_GAMES && m->games) {
        Model mm = *m;
        pictures(&mm, h);
        view_top(&top, &mm);
        view_bottom(&bot, &mm);
    } else {
        view_top(&top, m);
        view_bottom(&bot, m);
    }
    {
        const char *stamp = getenv("PREVIEW_STAMP");
        if (stamp && stamp[0])
            snprintf(path, sizeof path, "%s/%s-bananads-lan-%s-%dx%d-%s%s%s.png", dir, stamp, name, w, h,
                     strncmp(theme, "user:", 5) ? theme : theme + 5,
                     strcmp(lang, "en") ? "-" : "", strcmp(lang, "en") ? lang : "");
        else
            snprintf(path, sizeof path, "%s/bananads-lan-%s-%dx%d-%s%s%s.png", dir, name, w, h,
                     strncmp(theme, "user:", 5) ? theme : theme + 5,
                     strcmp(lang, "en") ? "-" : "", strcmp(lang, "en") ? lang : "");
    }
    ok = px_png_write(path, buf, w, h * 2 + gap);
    if (ok) printf("%s\n", path);
    free(buf);
    return ok;
}

int game_is_favorite(const char *rom) { (void)rom; return 0; }

static unsigned preview_banner_sig(const char *rom)
{
    PxImage img;
    unsigned h = 2166136261u;
    if (!px_nds_icon(rom, &img)) return 0;
    for (size_t i = 0; i < (size_t)img.w * (size_t)img.h; i++) h ^= img.px[i], h *= 16777619u;
    px_image_free(&img);
    return h;
}

static int preview_leaf_eq(const char *path, const char *leaf)
{
    const char *s = strrchr(path, '/');
    return s && strcasecmp(s + 1, leaf) == 0;
}

static void preview_trim(char *s)
{
    char *p = s, *e;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    e = s + strlen(s);
    while (e > s && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
}

static int preview_games_from_list(const char *media, const char *list, char games[][GAME_PATH], int cap)
{
    FILE *f;
    char line[512];
    int n = 0;
    if (!media || !list || !list[0]) return 0;
    f = fopen(list, "r");
    if (!f) return 0;
    while (n < cap && fgets(line, sizeof line, f)) {
        preview_trim(line);
        if (!line[0] || line[0] == '#') continue;
        snprintf(games[n], GAME_PATH, "%s/%s", media, line);
        n++;
    }
    fclose(f);
    return n;
}

static int preview_cmp_leaf(const void *a, const void *b)
{
    return strcasecmp(*(const char *const *)a, *(const char *const *)b);
}

/* README library shots only (PREVIEW_README_ONLY=1): grid dark/en, list light/es. */
static int preview_readme_fill(const char *media, char games[][GAME_PATH], int *unique_out)
{
    static const char *names[PREVIEW_ROM_MAX];
    unsigned sigs[PREVIEW_ROM_MAX];
    char tmppath[GAME_PATH];
    int nnames = 0, n = 0, u = 0, i, j;
    DIR *d;
    struct dirent *e;
    if (!media) media = ".";
    d = opendir(media);
    if (!d) return 0;
    while (nnames < PREVIEW_ROM_MAX && (e = readdir(d))) {
        size_t len;
        if (e->d_name[0] == '.') continue;
        len = strlen(e->d_name);
        if (len < 5 || strcasecmp(e->d_name + len - 4, ".nds") != 0) continue;
        names[nnames++] = strdup(e->d_name);
    }
    closedir(d);
    qsort(names, (size_t)nnames, sizeof *names, preview_cmp_leaf);
    for (i = 0; i < nnames && n < PREVIEW_ROM_MAX; i++) {
        unsigned sig;
        snprintf(tmppath, sizeof tmppath, "%s/%s", media, names[i]);
        free((void *)names[i]);
        sig = preview_banner_sig(tmppath);
        if (!sig) continue;
        for (j = 0; j < u; j++)
            if (sigs[j] == sig) break;
        if (j < u) continue;
        sigs[u++] = sig;
        snprintf(games[n], GAME_PATH, "%s", tmppath);
        n++;
    }
    if (unique_out) *unique_out = u;
    /* 4+ grid pages: need ceil(n/GRID_ROWS) - GRID_COLS columns spare; list: n > 4*LIST_PAGE_STEP+GAME_ROWS */
    if (n < 26) {
        fprintf(stderr, "preview: need >=26 ROMs with banners (have %d)\n", n);
        return 0;
    }
    return n;
}

static int preview_readme_shots(const char *dir)
{
    const char *media = getenv("PREVIEW_MEDIA");
    static char readme_g[PREVIEW_ROM_MAX][GAME_PATH];
    Model m;
    int n, unique = 0, gsel, col, cols, ok = 1, k;
    if (!media) media = ".";
    n = preview_readme_fill(media, readme_g, &unique);
    if (n <= 0) return 1;
    gsel = 0;
    {
        const char *grid = getenv("PREVIEW_README_GRID_SEL");
        if (grid && grid[0])
            for (k = 0; k < n; k++)
                if (preview_leaf_eq(readme_g[k], grid)) { gsel = k; break; }
    }
    col = gsel / GRID_ROWS;
    cols = (n + GRID_ROWS - 1) / GRID_ROWS;
    m = base();
    snprintf(m.model, sizeof m.model, "rgds_plus");
    m.view = V_GAMES;
    m.games = readme_g;
    m.ngames = n;
    m.grid = 1;
    m.gsel = gsel;
    m.gleft = col > 2 ? col - 2 : 0;
    if (m.gleft > cols - GRID_COLS) m.gleft = cols > GRID_COLS ? cols - GRID_COLS : 0;
    m.gsave = 262144;
    m.gsave_time = time(NULL) - 3600 * 26;
    m.gsecs = 3600 * 12 + 60 * 43;
    m.gplays = 9;
    m.glast = time(NULL) - 3600 * 2;
    ok &= shot(dir, "readme-grid", 1024, 768, "dark", "yellow", "en", &m);
    gsel = 0;
    {
        const char *list = getenv("PREVIEW_README_LIST_SEL");
        if (list && list[0])
            for (k = 0; k < n; k++)
                if (preview_leaf_eq(readme_g[k], list)) { gsel = k; break; }
    }
    m = base();
    snprintf(m.model, sizeof m.model, "rgds_plus");
    m.view = V_GAMES;
    m.games = readme_g;
    m.ngames = n;
    m.grid = 0;
    m.gsel = gsel;
    m.gtop = gsel > 3 ? gsel - 3 : 0;
    if (m.gtop > n - GAME_ROWS) m.gtop = n > GAME_ROWS ? n - GAME_ROWS : 0;
    m.gsave = 393216;
    m.gsave_time = time(NULL) - 600;
    m.gsecs = 3600 * 41 + 60 * 5;
    m.glast = time(NULL) - 600;
    ok &= shot(dir, "readme-list", 1024, 768, "light", "yellow", "es", &m);
    fprintf(stderr, "preview: %d library ROMs, %d unique banner icons\n", n, unique);
    px_fonts_free();
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static const int sizes[2][2] = { { 640, 480 }, { 1024, 768 } };
    static const char *const themes[2] = { "dark", "light" };
    Model m;
    int s, t, ok = 1;
    if (!px_fonts_load(NULL)) { fprintf(stderr, "preview: set PIXEL_UI_FONTS to a folder with Pixelify Sans\n"); return 1; }
    if (getenv("PREVIEW_README_ONLY"))
        return preview_readme_shots(dir);
    for (s = 0; s < 2; s++)
        for (t = 0; t < 2; t++) {
            m = base();
            if (s == 0) snprintf(m.model, sizeof m.model, "rgds");
            ok &= shot(dir, "on", sizes[s][0], sizes[s][1], themes[t], "yellow", "en", &m);
        }
    m = base();
    m.enabled = 0;
    ok &= shot(dir, "off", 640, 480, "dark", "yellow", "es", &m);
    m = base();
    m.view = V_UNINSTALL;
    ok &= shot(dir, "uninstall", 1024, 768, "dark", "yellow", "es", &m);
    m = base();
    m.view = V_ABOUT;
    ok &= shot(dir, "about", 640, 480, "dark", "yellow", "es", &m);
    ok &= shot(dir, "about", 1024, 768, "light", "yellow", "en", &m);
    /* BananaOS's own themes (banana/themes, one JSON file each): light ones and dark ones */
    m = base();
    snprintf(m.model, sizeof m.model, "rgds");
    ok &= shot(dir, "on", 640, 480, "user:sakura", "yellow", "es", &m);
    ok &= shot(dir, "on", 640, 480, "user:comic-panel", "yellow", "en", &m);
    ok &= shot(dir, "on", 640, 480, "user:swiss-poster", "yellow", "en", &m);
    ok &= shot(dir, "on", 640, 480, "user:paper-notebook", "yellow", "es", &m);
    ok &= shot(dir, "on", 640, 480, "user:ocean", "yellow", "en", &m);
    m.enabled = 0;
    ok &= shot(dir, "off", 640, 480, "user:sunset", "yellow", "es", &m);
    /* the updater: what the Updates row says, the question before it installs, its progress, its end */
    m = base();
    m.cursor = ROW_UPDATES;
    ok &= shot(dir, "update-uptodate", 1024, 768, "dark", "yellow", "en", &m);
    m.upd.state = UPD_AVAILABLE;
    snprintf(m.upd.latest, sizeof m.upd.latest, "0.4.1");
    ok &= shot(dir, "update-available", 640, 480, "dark", "yellow", "es", &m);
    ok &= shot(dir, "update-available", 640, 480, "light", "yellow", "en", &m);
    m.view = V_UPDATE;
    ok &= shot(dir, "update-confirm", 640, 480, "light", "yellow", "es", &m);
    m.view = V_MAIN;
    m.upd.state = UPD_DOWNLOADING;
    m.upd.installing = 1;
    m.upd.pid = 1;                 /* (a running job, for the greyed keys) */
    m.upd.pct = 46;
    ok &= shot(dir, "update-downloading", 640, 480, "dark", "yellow", "es", &m);
    ok &= shot(dir, "update-downloading", 1024, 768, "light", "yellow", "en", &m);
    m.upd.state = UPD_INSTALLING;
    ok &= shot(dir, "update-installing", 640, 480, "dark", "yellow", "en", &m);
    m.upd.pid = 0;
    m.upd.state = UPD_FAILED;
    snprintf(m.upd.msg, sizeof m.upd.msg, "Download damaged");
    ok &= shot(dir, "update-failed", 640, 480, "dark", "yellow", "es", &m);
    m.upd.state = UPD_OFFLINE;
    m.upd.installing = 0;
    m.upd.msg[0] = 0;
    ok &= shot(dir, "update-offline", 640, 480, "light", "yellow", "en", &m);
    m.upd.state = UPD_DONE;
    m.upd.installing = 1;
    snprintf(m.upd.current, sizeof m.upd.current, "0.4.1");
    snprintf(m.version, sizeof m.version, "0.4.1");
    m.view = V_UPDATED;
    ok &= shot(dir, "update-done", 640, 480, "dark", "yellow", "es", &m);
    /* the library, where the app opens: a grid scrolled sideways (X: a list), the picked game's cover on top */
    {
        static char games[PREVIEW_ROM_MAX][GAME_PATH];
        const char *media = getenv("PREVIEW_MEDIA");
        const char *list = getenv("PREVIEW_LIBRARY_FILE");
        int ng;
        if (!list || !list[0]) list = "preview-library.list";
        ng = preview_games_from_list(media ? media : ".", list, games, PREVIEW_ROM_MAX);
        if (ng < 1) {
            fprintf(stderr, "preview: set PREVIEW_LIBRARY_FILE to a list of ROM basenames\n");
            px_fonts_free();
            return 1;
        }
        m = base();
        snprintf(m.model, sizeof m.model, "rgds");
        m.view = V_GAMES;
        m.games = games;
        m.ngames = ng;
        m.grid = 1;
        m.gsel = ng > 2 ? 2 : 0;
        m.gsave = 262144;
        m.gsave_time = time(NULL) - 3600 * 26;
        m.gsecs = 3600 * 12 + 60 * 43;
        m.gplays = 9;
        m.glast = time(NULL) - 3600 * 2;
        ok &= shot(dir, "library-grid", 640, 480, "dark", "yellow", "es", &m);
        ok &= shot(dir, "library-grid", 1024, 768, "light", "yellow", "en", &m);
        ok &= shot(dir, "library-grid", 640, 480, "user:sakura", "yellow", "es", &m);
        /* further along: the cover-less game shows the icon from its ROM */
        m.gsel = 5;
        m.gleft = 1;
        m.gsave = 524288;
        m.gsave_time = time(NULL) - 86400 * 12;
        m.gsecs = 60 * 35;
        m.glast = time(NULL) - 86400 * 12;
        ok &= shot(dir, "library-grid-icon", 640, 480, "dark", "blue", "es", &m);
        ok &= shot(dir, "library-grid-icon", 1024, 768, "dark", "yellow", "es", &m);
        m.gsel = 8;
        m.gleft = 2;
        m.gsave = -1;
        m.gsecs = 0;
        m.glast = 0;
        m.enabled = 0;
        ok &= shot(dir, "library-grid-end", 640, 480, "light", "yellow", "es", &m);
        /* the list */
        m = base();
        snprintf(m.model, sizeof m.model, "rgds");
        m.view = V_GAMES;
        m.games = games;
        m.ngames = ng;
        m.grid = 0;
        m.gsel = ng > 6 ? 6 : 0;
        m.gtop = 3;
        m.gsave = 393216;
        m.gsave_time = time(NULL) - 600;
        m.gsecs = 3600 * 41 + 60 * 5;
        m.glast = time(NULL) - 600;
        ok &= shot(dir, "library-list", 640, 480, "dark", "yellow", "es", &m);
        ok &= shot(dir, "library-list", 1024, 768, "light", "yellow", "en", &m);
        {
            const char *mrom = getenv("PREVIEW_MARQUEE_ROM");
            const char *mtitle = getenv("PREVIEW_MARQUEE_TITLE");
            int slot = 4;
            const char *sslot = getenv("PREVIEW_MARQUEE_SLOT");
            if (sslot && sslot[0]) slot = atoi(sslot);
            if (slot < 0) slot = 0;
            if (slot >= ng) slot = ng - 1;
            if (mrom && mrom[0])
                snprintf(games[slot], GAME_PATH, "%s/%s", media ? media : ".", mrom);
            m.gsel = slot;
            m.gtop = slot > 2 ? slot - 2 : 0;
            if (mtitle && mtitle[0]) px_marquee_preview(mtitle, 2800);
        }
        ok &= shot(dir, "library-list-marquee", 640, 480, "dark", "yellow", "es", &m);
        m.upd.state = UPD_AVAILABLE;
        snprintf(m.upd.latest, sizeof m.upd.latest, "0.4.1");
        ok &= shot(dir, "library-update", 640, 480, "dark", "yellow", "es", &m);
        m.ngames = 0;
        ok &= shot(dir, "library-empty", 640, 480, "dark", "yellow", "es", &m);
    }
    px_fonts_free();
    return ok ? 0 : 1;
}

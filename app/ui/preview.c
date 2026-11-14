/* Preview: BananaDS LAN's panels as PNG files, on a computer (build/preview.sh). Both panels in one picture, top
   over bottom, for each panel size (RG DS 640x480, RG DS Plus 1024x768) and theme. Nothing here runs on a handheld. */
#include "pixel_png.h"
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static Model base(void)
{
    Model m;
    memset(&m, 0, sizeof m);
    m.supported = 1;
    m.enabled = 1;
    m.mounted = 1;
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
    static PxImage thumbs[64], art;
    static signed char st[64];
    float s = h / 480.0f;
    int i;
    for (i = 0; i < 64; i++) if (st[i] == 1) px_image_free(&thumbs[i]);
    memset(st, 0, sizeof st);
    px_image_free(&art);
    m->thumbs = thumbs;
    m->tstate = st;
    for (i = 0; i < m->ngames && i < 64; i++) {
        char box[GAME_PATH + 64];
        int ok = 0;
        ok = px_nds_icon(m->games[i], &thumbs[i]);
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

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static const int sizes[2][2] = { { 640, 480 }, { 1024, 768 } };
    static const char *const themes[2] = { "dark", "light" };
    Model m;
    int s, t, ok = 1;
    if (!px_fonts_load(NULL)) { fprintf(stderr, "preview: set PIXEL_UI_FONTS to a folder with Pixelify Sans\n"); return 1; }
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
    m.supported = 0;
    m.enabled = 0;
    snprintf(m.os, sizeof m.os, "0.5.4");
    snprintf(m.reason, sizeof m.reason, "the app's BananaDS (emu/bananads) is missing: install the app again");
    m.cursor = ROW_UPDATES;
    ok &= shot(dir, "emu-missing", 640, 480, "dark", "blue", "en", &m);
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
        static const char *names[10] = {
            "Advance Wars - Dual Strike.nds", "Chrono Trigger.nds", "Mario Kart DS.nds", "New Super Mario Bros.nds",
            "Picross 3D.nds", "Pokemon - Edicion Negra 2.nds", "Pokemon - Edicion Platino.nds",
            "Professor Layton and the Curious Village.nds", "Tetris DS.nds", "The Legend of Zelda - Phantom Hourglass.nds" };
        static char games[10][GAME_PATH];
        const char *media = getenv("PREVIEW_MEDIA");
        int i;
        for (i = 0; i < 10; i++) snprintf(games[i], GAME_PATH, "%s/%s", media ? media : ".", names[i]);
        m = base();
        snprintf(m.model, sizeof m.model, "rgds");
        m.view = V_GAMES;
        m.games = games;
        m.ngames = 10;
        m.emu_ok = 1;
        m.grid = 1;
        m.gsel = 2;
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
        m.ngames = 10;
        m.emu_ok = 1;
        m.grid = 0;
        m.gsel = 6;
        m.gtop = 3;
        m.gsave = 393216;
        m.gsave_time = time(NULL) - 600;
        m.gsecs = 3600 * 41 + 60 * 5;
        m.glast = time(NULL) - 600;
        ok &= shot(dir, "library-list", 640, 480, "dark", "yellow", "es", &m);
        ok &= shot(dir, "library-list", 1024, 768, "light", "yellow", "en", &m);
        m.upd.state = UPD_AVAILABLE;
        snprintf(m.upd.latest, sizeof m.upd.latest, "0.4.1");
        ok &= shot(dir, "library-update", 640, 480, "dark", "yellow", "es", &m);
        m.ngames = 0;
        ok &= shot(dir, "library-empty", 640, 480, "dark", "yellow", "es", &m);
    }
    m = base();
    m.view = V_NEST;
    ok &= shot(dir, "nest", 640, 480, "light", "green", "en", &m);
    px_fonts_free();
    return ok ? 0 : 1;
}

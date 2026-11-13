/* Preview: BananaDS LAN's panels as PNG files, on a computer (build/preview.sh). Both panels in one picture, top
   over bottom, for each panel size (RG DS 640x480, RG DS Plus 1024x768) and theme. Nothing here runs on a handheld. */
#include "pixel_png.h"
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Model base(void)
{
    Model m;
    memset(&m, 0, sizeof m);
    m.supported = 1;
    m.enabled = 1;
    m.mounted = 1;
    snprintf(m.emulator, sizeof m.emulator, "bananads");
    snprintf(m.os, sizeof m.os, "0.5.3.4");
    snprintf(m.want_os, sizeof m.want_os, "0.5.3.4");
    snprintf(m.version, sizeof m.version, "0.3.0");
    snprintf(m.model, sizeof m.model, "rgds_plus");
    snprintf(m.ip, sizeof m.ip, "192.168.18.184");
    m.battery = 82;
    snprintf(m.clock, sizeof m.clock, "14:30");
    return m;
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
    px_canvas(&top, buf, w, h);
    px_canvas(&bot, buf + (size_t)w * (size_t)(h + gap), w, h);
    {
        int i;
        for (i = 0; i < w * gap; i++) buf[(size_t)w * (size_t)h + (size_t)i] = 0x000000;
    }
    view_top(&top, m);
    view_bottom(&bot, m);
    snprintf(path, sizeof path, "%s/bananads-lan-%s-%dx%d-%s%s%s.png", dir, name, w, h,
             strncmp(theme, "user:", 5) ? theme : theme + 5,
             strcmp(lang, "en") ? "-" : "", strcmp(lang, "en") ? lang : "");
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
    snprintf(m.reason, sizeof m.reason, "BananaOS 0.5.4 is not supported (this app is for 0.5.3.4)");
    snprintf(m.emulator, sizeof m.emulator, "drastic");
    m.cursor = ROW_UPDATES;
    ok &= shot(dir, "unsupported", 640, 480, "dark", "blue", "en", &m);
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
    m = base();
    m.view = V_NEST;
    ok &= shot(dir, "nest", 640, 480, "light", "green", "en", &m);
    px_fonts_free();
    return ok ? 0 : 1;
}

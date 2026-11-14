/* BananaDS LAN: the two panels in the menu's look (pixel.c).
   Top: what the app sees (local multiplayer on or off, this console, BananaOS, the network, its own BananaDS, the version).
   Bottom: a list as Settings and Bshare draw theirs, with the keys along the bottom. */
#include "view.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

Hit g_hits[MAX_HITS];
int g_nhits;

/* Spanish: neutral, and the words BananaOS's own translation uses (Encendido, Atrás, Consola...) */
const PxTr view_tr[] = {
    { "Local multiplayer", "Multijugador local" },
    { "Updates", "Actualizaciones" },
    { "Uninstall", "Desinstalar" },
    { "On", "Encendido" },
    { "Off", "Apagado" },
    { "Exit", "Salir" },
    { "Change", "Cambiar" },
    { "Cancel", "Cancelar" },
    { "Back", "Atrás" },
    { "Move it", "Moverla" },
    { "Ready", "Listo" },
    { "Emulator missing", "Falta el emulador" },
    { "This console", "Esta consola" },
    { "Network", "Red" },
    { "DS emulator", "Emulador de DS" },
    { "App version", "Versión de la app" },
    { "No network", "Sin red" },
    { "Unknown", "Desconocida" },
    { "Games you open in Play look for other consoles on your Wi-Fi as they start. A game's menu can change it too.",
      "Los juegos que abras en Jugar buscan otras consolas en tu Wi-Fi al empezar. También puedes cambiarlo en el menú del juego." },
    { "Games you open in Play start with local multiplayer off. Turn it on in the game's menu when you need it.",
      "Los juegos que abras en Jugar empiezan sin multijugador local. Actívalo en el menú del juego cuando lo necesites." },
    { "Open Play, pick the same game on each console and go to its wireless mode: the consoles find each other.",
      "Entra a Jugar, elige el mismo juego en cada consola y entra a su modo inalámbrico: las consolas se encuentran solas." },
    { "About", "Acerca de" },
    { "Version", "Versión" },
    { "Made by", "Hecha por" },
    { "Source code", "Código fuente" },
    { "Credits", "Créditos" },
    { "Local wireless multiplayer for DS games on BananaOS: two consoles on the same Wi-Fi play together, as on a real DS.",
      "Multijugador inalámbrico local para juegos de DS en BananaOS: dos consolas en el mismo Wi-Fi juegan juntas, como en una DS de verdad." },
    { "BananaOS and BananaLinux by JorreFog and the Banana contributors (MIT). BananaDS (SuperDrastic) and melonDS (GPLv3). Pixelify Sans (OFL).",
      "BananaOS y BananaLinux, de JorreFog y los colaboradores de Banana (MIT). BananaDS (SuperDrastic) y melonDS (GPLv3). Pixelify Sans (OFL)." },
    { "An independent app, not part of BananaOS. Built and tested on the RG DS and the RG DS Plus. Free software under the GPLv3.",
      "Una app independiente, no es parte de BananaOS. Hecha y probada en la RG DS y la RG DS Plus. Software libre bajo la GPLv3." },
    { "Uninstall BananaDS LAN?", "¿Desinstalar BananaDS LAN?" },
    { "Turns it off and deletes this app.", "Lo apaga y borra esta app." },
    { "One folder too deep.", "Una carpeta de más." },
    { "A moves it up one folder.", "A la sube una carpeta." },
    { "Moved.", "Movida." },
    { "Open BananaDS LAN again.", "Abre BananaDS LAN de nuevo." },
    { "Play", "Jugar" },
    { "Games", "Juegos" },
    { "%d games", "%d juegos" },
    { "1 game", "1 juego" },
    { "No games", "Sin juegos" },
    { "Game", "Juego" },
    { "Save", "Partida guardada" },
    { "None yet", "Todavía no" },
    { "Emulator", "Emulador" },
    { "Open", "Abrir" },
    { "Missing", "No está" },
    { "Folder", "Carpeta" },
    { "Your DS games, played on this app's own BananaDS with local multiplayer. BananaOS's BananaDS stays as it is.",
      "Tus juegos de DS, en el BananaDS propio de esta app con multijugador local. El BananaDS de BananaOS no cambia." },
    { "Saves are shared with BananaOS's BananaDS. Save states are kept apart.",
      "Las partidas guardadas se comparten con el BananaDS de BananaOS. Los estados guardados van aparte." },
    { "No DS games in roms/nds.", "No hay juegos de DS en roms/nds." },
    { "Open the same game on each console and go to its wireless mode.",
      "Abre el mismo juego en cada consola y entra a su modo inalámbrico." },
    { "Page", "Página" },
    { "Settings", "Ajustes" },
    { "List", "Lista" },
    { "Grid", "Grilla" },
    { "Order: A-Z", "Orden: A-Z" },
    { "Order: Favorites", "Orden: Favoritos" },
    { "Order: Recent", "Orden: Recientes" },
    { "Add favorite", "Añadir a favoritos" },
    { "Remove favorite", "Quitar de favoritos" },
    { "Favorite", "Favorito" },
    { "Play time", "Tiempo de juego" },
    { "Last played", "Última vez" },
    { "Not played yet", "Sin jugar todavía" },
    { "Never", "Nunca" },
    { "Under a minute", "Menos de un minuto" },
    { "Today, %s", "Hoy, %s" },
    { "Yesterday, %s", "Ayer, %s" },
    { "Jan", "ene" }, { "Feb", "feb" }, { "Mar", "mar" }, { "Apr", "abr" }, { "May", "may" }, { "Jun", "jun" },
    { "Jul", "jul" }, { "Aug", "ago" }, { "Sep", "sept" }, { "Oct", "oct" }, { "Nov", "nov" }, { "Dec", "dic" },
    { "Put your .nds games in roms/nds on the card (or the second card) and open the app again.",
      "Pon tus juegos .nds en roms/nds de la tarjeta (o de la segunda tarjeta) y abre la app de nuevo." },
    { "This app's BananaDS is missing: reinstall the app.", "Falta el BananaDS de esta app: vuelve a instalarla." },
    { "v%s is out: START, Updates.", "Ya salió la v%s: START, Actualizaciones." },
    { "Pick the same game on each console and go to its wireless mode: the consoles find each other.",
      "Elige el mismo juego en cada consola y entra a su modo inalámbrico: las consolas se encuentran solas." },
    { "Games you open here look for other consoles on your Wi-Fi as they start. A game's menu can change it too.",
      "Los juegos que abras aquí buscan otras consolas en tu Wi-Fi al empezar. También puedes cambiarlo en el menú del juego." },
    { "Games you open here start with local multiplayer off. Turn it on in the game's menu when you need it.",
      "Los juegos que abras aquí empiezan sin multijugador local. Actívalo en el menú del juego cuando lo necesites." },
    { NULL, NULL }
};

#define T(s) px_tr(s)

static void games_top(PxCanvas *c, const Model *m);
static void games_bottom(PxCanvas *c, const Model *m);

static void add_hit(const PxCanvas *c, float x, float y, float w, float h, int id, int arg)
{
    if (g_nhits >= MAX_HITS) return;
    g_hits[g_nhits].r.x = px_u(c, x);
    g_hits[g_nhits].r.y = px_u(c, y);
    g_hits[g_nhits].r.w = px_u(c, x + w) - g_hits[g_nhits].r.x;
    g_hits[g_nhits].r.h = px_u(c, y + h) - g_hits[g_nhits].r.y;
    g_hits[g_nhits].id = id;
    g_hits[g_nhits].arg = arg;
    g_nhits++;
}

static void add_face_hit(PxRect r, int id)
{
    if (g_nhits >= MAX_HITS) return;
    g_hits[g_nhits].r = r;
    g_hits[g_nhits].id = id;
    g_hits[g_nhits].arg = 0;
    g_nhits++;
}

int view_row_enabled(const Model *m, int row)
{
    (void)m;
    (void)row;
    return 1;
}

static const char *model_name(const char *id)
{
    if (!strcmp(id, "rgds")) return "RG DS";
    if (!strcmp(id, "rgds_plus")) return "RG DS Plus";
    return T("Unknown");
}

/* a value over its label, as the game page's stat block */
static void stat(PxCanvas *c, float x, float y, float w, const char *label, const char *value, uint32_t rgb)
{
    px_text_fit(c, PX_MEDIUM, 22, x, y, value, rgb, 0, w, PX_LEFT);
    px_label(c, x, y + 28, label, px.muted);
}

void view_top(PxCanvas *c, const Model *m)
{
    char buf[128], net[96];
    const char *hero = m->enabled ? T("On") : T("Off");
    const char *msg = NULL;
    uint32_t msg_rgb = px.ink;
    float hw, cw;
    if (m->view == V_GAMES) { games_top(c, m); return; }
    px_screen(c);
    px_status(c, "BananaDS LAN", m->battery, m->charging, m->clock);
    px_sheet(c, 18, 60, 604, 406);

    px_label(c, 44, 84, T("Local multiplayer"), px.ink);
    /* the state, big: green on, grey off */
    hw = px_badge(c, 42, 106, 52, hero, m->enabled ? PX_BADGE_ON : PX_BADGE_OFF);
    /* beside it: ready, or why not */
    snprintf(buf, sizeof buf, "%s", m->supported ? T("Ready") : T("Emulator missing"));
    cw = px_text_w(c, PX_MEDIUM, 15, buf, PX_UPPER | PX_TRACK) + 24;
    px_chip(c, 42 + hw + 18, 117, cw, 30, 0);
    px_text(c, PX_MEDIUM, 15, 42 + hw + 30, 122, buf, m->supported ? px.ink : px.warn, PX_UPPER | PX_TRACK);

    px_line_h(c, 42, 168, 556, 2, px.edge, 70);

    stat(c, 42, 186, 260, T("This console"), model_name(m->model), px.ink);
    stat(c, 330, 186, 260, "BananaOS", m->os[0] ? m->os : "?", px.ink);

    if (!m->ip[0]) snprintf(net, sizeof net, "%s", T("No network"));
    else if (!strncmp(m->ip, "10.42.0.", 8)) snprintf(net, sizeof net, "Direct Link  ·  %s", m->ip);
    else snprintf(net, sizeof net, "Wi-Fi  ·  %s", m->ip);
    stat(c, 42, 254, 260, T("Network"), net, m->ip[0] ? px.ink : px.muted);
    stat(c, 330, 254, 260, T("DS emulator"), m->supported ? "BananaDS LAN" : T("Emulator missing"),
         m->supported ? px.ink : px.warn);

    if (m->upd.state == UPD_AVAILABLE) {
        char avail[64];
        snprintf(avail, sizeof avail, T("v%s available"), m->upd.latest);
        snprintf(buf, sizeof buf, "%s  ·  %s", m->version[0] ? m->version : "?", avail);
        stat(c, 42, 322, 556, T("App version"), buf, px.ink);
    } else stat(c, 42, 322, 260, T("App version"), m->version[0] ? m->version : "?", px.ink);

    px_line_h(c, 42, 380, 556, 2, px.edge, 70);
    if (m->err[0]) { msg = m->err; msg_rgb = px.warn; }
    else if (!m->supported && m->reason[0]) { msg = m->reason; msg_rgb = px.warn; }
    else msg = T("Pick the same game on each console and go to its wireless mode: the consoles find each other.");
    px_text_wrap(c, PX_REGULAR, 18, 42, 392, msg, msg_rgb, 556, 2, 24);
}

void view_game_name(const char *path, char *dst, int n)
{
    const char *b = strrchr(path, '/'), *dot;
    int len;
    b = b ? b + 1 : path;
    dot = strrchr(b, '.');
    len = dot && dot > b ? (int)(dot - b) : (int)strlen(b);
    if (len > n - 1) len = n - 1;
    memcpy(dst, b, (size_t)len);
    dst[len] = 0;
}

/* "3 h 12 min", "45 min", "Under a minute" */
static void fmt_time(char *buf, size_t n, long secs)
{
    if (secs >= 3600) snprintf(buf, n, "%ld h %ld min", secs / 3600, secs % 3600 / 60);
    else if (secs >= 60) snprintf(buf, n, "%ld min", secs / 60);
    else snprintf(buf, n, "%s", T("Under a minute"));
}

/* today and yesterday by the hour (the clock as settings.conf has it), older by the date */
static void fmt_when(char *buf, size_t n, time_t t, int clock12)
{
    static const char *mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    time_t now = time(NULL);
    struct tm a, b;
    char hm[16];
    long days;
    localtime_r(&t, &a);
    localtime_r(&now, &b);
    if (clock12) {
        int h = a.tm_hour % 12;
        snprintf(hm, sizeof hm, "%d:%02d %s", h ? h : 12, a.tm_min, a.tm_hour < 12 ? "AM" : "PM");
    } else snprintf(hm, sizeof hm, "%02d:%02d", a.tm_hour, a.tm_min);
    {
        struct tm da = a, db = b;
        da.tm_hour = db.tm_hour = 12;
        da.tm_min = db.tm_min = da.tm_sec = db.tm_sec = 0;
        days = (long)((mktime(&db) - mktime(&da) + 43200) / 86400);
    }
    if (days == 0) snprintf(buf, n, T("Today, %s"), hm);
    else if (days == 1) snprintf(buf, n, T("Yesterday, %s"), hm);
    else snprintf(buf, n, "%d %s %d", a.tm_mday, T(mon[a.tm_mon]), a.tm_year + 1900);
}

/* a cover, or the tile's place for one: the game's icon, else its initial on the card */
static void game_image(PxCanvas *c, const PxImage *img, float x, float y, float w, float h, const char *name, int sel)
{
    if (img && img->px) { px_image_draw(c, x, y, w, h, img); return; }
    {
        char ch[2] = { name[0] ? (char)toupper((unsigned char)name[0]) : '?', 0 };
        float sz = h * 0.5f;
        px_rect_a(c, x, y, w, h, sel ? 0xffffff : px.ink, 22);
        px_text(c, PX_MEDIUM, sz, x + (w - px_text_w(c, PX_MEDIUM, sz, ch, 0)) / 2, y + (h - sz) / 2 - sz * 0.12f, ch,
                sel ? px.on_pick_sub : px.muted, 0);
    }
}

/* top: the picked game's cover (or its icon), its name, play time, last played and save */
static void games_top(PxCanvas *c, const Model *m)
{
    char name[GAME_PATH], buf[96], when[48];
    px_screen(c);
    px_status(c, "BananaDS LAN", m->battery, m->charging, m->clock);
    px_sheet(c, 18, 60, 604, 406);
    if (m->ngames <= 0) {
        px_label(c, 44, 84, T("Games"), px.ink);
        px_text_wrap(c, PX_MEDIUM, 26, 42, 110, T("No DS games in roms/nds."), px.warn, 556, 2, 32);
        px_text_wrap(c, PX_REGULAR, 18, 42, 190,
                     T("Put your .nds games in roms/nds on the card (or the second card) and open the app again."),
                     px.muted, 556, 3, 24);
        return;
    }
    view_game_name(m->games[m->gsel], name, (int)sizeof name);
    game_image(c, m->art_game == m->gsel && m->art.px ? &m->art : NULL, 40, 80, ART_W, ART_H, name, 0);

    px_label(c, 284, 84, T("Game"), px.muted);
    px_text_wrap(c, PX_MEDIUM, 26, 282, 104, name, px.ink, 316, 2, 31);
    px_line_h(c, 284, 176, 314, 2, px.edge, 70);

    if (m->gsecs > 0) {
        fmt_time(buf, sizeof buf, m->gsecs);
        stat(c, 284, 190, 314, T("Play time"), buf, px.ink);
    } else stat(c, 284, 190, 314, T("Play time"), T("Not played yet"), px.muted);
    if (m->glast > 0) {
        fmt_when(when, sizeof when, m->glast, m->clock12);
        stat(c, 284, 246, 314, T("Last played"), when, px.ink);
    } else stat(c, 284, 246, 314, T("Last played"), T("Never"), px.muted);
    if (m->gsave >= 0) {
        char size[24];
        if (m->gsave >= 1024) snprintf(size, sizeof size, "%ld KB", m->gsave / 1024);
        else snprintf(size, sizeof size, "%ld B", m->gsave);
        fmt_when(when, sizeof when, m->gsave_time, m->clock12);
        snprintf(buf, sizeof buf, "%s  ·  %s", size, when);
        stat(c, 284, 302, 314, T("Save"), buf, px.ink);
    } else stat(c, 284, 302, 314, T("Save"), T("None yet"), px.muted);

    px_line_h(c, 42, 360, 556, 2, px.edge, 70);
    {
        /* local multiplayer, as it is for the next game, and the network it would use */
        const char *st = m->enabled ? T("On") : T("Off");
        float bw = px_text_w(c, PX_MEDIUM, 14, st, PX_UPPER | PX_TRACK) + 22, lw;
        px_label(c, 42, 381, T("Local multiplayer"), px.muted);
        lw = px_text_w(c, PX_MEDIUM, 13, T("Local multiplayer"), PX_UPPER | PX_TRACK);
        px_chip(c, 42 + lw + 14, 374, bw, 28, 0);
        px_text(c, PX_MEDIUM, 14, 42 + lw + 25, 379, st, m->enabled ? px.ink : px.muted, PX_UPPER | PX_TRACK);
        if (!m->ip[0]) snprintf(buf, sizeof buf, "%s", T("No network"));
        else if (!strncmp(m->ip, "10.42.0.", 8)) snprintf(buf, sizeof buf, "Direct Link  ·  %s", m->ip);
        else snprintf(buf, sizeof buf, "Wi-Fi  ·  %s", m->ip);
        px_text_fit(c, PX_REGULAR, 17, 598, 377, buf, m->ip[0] ? px.muted : px.warn, 0, 556 - lw - bw - 40, PX_RIGHT);
    }
    if (m->err[0]) px_text_wrap(c, PX_REGULAR, 17, 42, 414, m->err, px.warn, 556, 2, 22);
    else if (!m->emu_ok) px_text_wrap(c, PX_REGULAR, 17, 42, 414, T("This app's BananaDS is missing: reinstall the app."), px.warn, 556, 2, 22);
    else if (m->upd.state == UPD_AVAILABLE) {
        snprintf(buf, sizeof buf, T("v%s is out: START, Updates."), m->upd.latest);
        px_text_fit(c, PX_MEDIUM, 17, 42, 416, buf, px.accent.base, 0, 556, PX_LEFT);
    } else
        px_text_wrap(c, PX_REGULAR, 17, 42, 414, T("Open the same game on each console and go to its wireless mode."),
                     px.muted, 556, 2, 22);
}

static const char *sort_label(int sort)
{
    if (sort == SORT_FAV) return T("Order: Favorites");
    if (sort == SORT_RECENT) return T("Order: Recent");
    return T("Order: A-Z");
}

/* top of the library panel: navigation hints and the game count in the middle */
static void library_top_legend(PxCanvas *c, const Model *m)
{
    char count[24];
    const char *sort = sort_label(m->sort);
    const char *view = m->grid ? T("List") : T("Grid");
    float y = 12, cy = y + 18, mid = 320, cw;
    uint32_t ink = px.ink, dim = px.dim;
    px_bubble(c, 18, y, 604, 42, 0);
    if (m->ngames > 0) {
        int vert = !m->grid;
        px_keycap(c, 34, y + 8, "L");
        px_triangle(c, 62, cy, 10, vert ? -2 : -1, ink);
        px_keycap(c, 78, y + 8, "L2");
        px_text_fit(c, PX_MEDIUM, 15, 114, y + 12, sort, ink, 0, 168, PX_LEFT);
        add_hit(c, 74, y + 4, 210, 34, HIT_SORT, 0);
        snprintf(count, sizeof count, "%02d / %02d", m->gsel + 1, m->ngames);
        cw = px_text_w(c, PX_MEDIUM, 19, count, 0);
        px_text(c, PX_MEDIUM, 19, mid - cw / 2, y + 11, count, px.muted, 0);
        {
            float x, vw, kw, hx0;
            x = 614;
            kw = px_keycap_w(c, "R");
            x -= kw;
            px_keycap(c, x, y + 8, "R");
            x -= 8;
            px_triangle(c, x - 5, cy, 10, vert ? 2 : 1, ink);
            x -= 16;
            kw = px_keycap_w(c, "R2");
            x -= kw;
            px_keycap(c, x, y + 8, "R2");
            x -= 8;
            vw = px_text_w(c, PX_MEDIUM, 15, view, 0);
            hx0 = x - vw - 4;
            x -= vw;
            px_text(c, PX_MEDIUM, 15, x, y + 12, view, ink, 0);
            add_hit(c, hx0, y + 4, 614 - hx0, 34, HIT_VIEW, 0);
        }
    } else {
        snprintf(count, sizeof count, "%s", T("No games"));
        cw = px_text_w(c, PX_MEDIUM, 19, count, 0);
        px_text(c, PX_MEDIUM, 19, mid - cw / 2, y + 11, count, dim, 0);
    }
    (void)dim;
}

/* bottom face row only */
static void library_bottom_faces(PxCanvas *c, const Model *m)
{
    PxRect r;
    float y = LIB_FACE_Y, bw, aw;
    int disfav = m->ngames <= 0;
    int fav_on = m->gfav && !disfav;
    px_face(c, 18, y, PX_LEFT, "B", T("Exit"), 0, &r, 0, 0);
    add_face_hit(r, HIT_BACK);
    bw = px_face_w(c, "B", T("Exit"));
    px_face(c, 18 + bw + 8, y, PX_LEFT, "START", T("Settings"), 0, &r, 0, 0);
    add_face_hit(r, HIT_START);
    px_face(c, 622, y, PX_RIGHT, "A", T("Play"), m->ngames <= 0 || !m->emu_ok, &r, 0, 0);
    add_face_hit(r, HIT_OK);
    aw = px_face_w(c, "A", T("Play"));
    px_face(c, 622 - aw - 8, y, PX_RIGHT, "Y", T("Favorite"), disfav, &r, 0, fav_on);
    add_face_hit(r, HIT_Y);
}

/* bottom: the library, as a grid scrolled sideways or as a list */
static void games_bottom(PxCanvas *c, const Model *m)
{
    char name[GAME_PATH];
    PxRect rail;
    int i;
    float grid_bot;
    g_nhits = 0;
    px_screen(c);
    library_top_legend(c, m);
    if (m->ngames <= 0) {
        px_card(c, 18, LIB_TOP, 604, 120, 0, 0);
        px_text_wrap(c, PX_MEDIUM, 22, 40, LIB_TOP + 20, T("No DS games in roms/nds."), px.ink, 560, 1, 28);
        px_text_wrap(c, PX_REGULAR, 17, 40, LIB_TOP + 56,
                     T("Put your .nds games in roms/nds on the card (or the second card) and open the app again."),
                     px.muted, 560, 2, 23);
    } else if (m->grid) {
        int cols = (m->ngames + GRID_ROWS - 1) / GRID_ROWS;
        for (i = 0; i < GRID_COLS * GRID_ROWS; i++) {
            int col = m->gleft + i / GRID_ROWS, rw = i % GRID_ROWS, g = col * GRID_ROWS + rw, sel;
            float x, y;
            if (col >= cols || g >= m->ngames) continue;
            sel = g == m->gsel;
            x = 18 + (i / GRID_ROWS) * (TILE_W + TILE_GAP);
            y = LIB_TOP + rw * (TILE_H + TILE_GAP);
            view_game_name(m->games[g], name, (int)sizeof name);
            px_card(c, x, y, TILE_W, TILE_H, sel, 0);
            game_image(c, m->tstate && m->tstate[g] == 1 ? &m->thumbs[g] : NULL, x + 8, y + 8, TILE_IMG_W, TILE_IMG_H,
                       name, sel);
            if (game_is_favorite(m->games[g]))
                px_fav_mark(c, x + TILE_W - FAV_MARK - 6, y + 6, FAV_MARK);
            add_hit(c, x, y, TILE_W, TILE_H, HIT_ROW, g);
        }
        grid_bot = LIB_TOP + GRID_TILES_H + LIB_GRID_GAP;
        if (cols > GRID_COLS || m->ngames > 1) {
            PxRect track, thumb;
            px_hrail(c, 18, grid_bot, LIB_W, m->gsel, m->ngames, NULL, &track, &thumb);
            g_hits[g_nhits].r = track;
            g_hits[g_nhits].id = HIT_RAIL_TRACK;
            g_hits[g_nhits].arg = 0;
            g_nhits++;
            g_hits[g_nhits].r = thumb;
            g_hits[g_nhits].id = HIT_RAIL_THUMB;
            g_hits[g_nhits].arg = 0;
            g_nhits++;
        }
    } else {
        static int mq_gsel = -1;
        float list_h = LIB_FACE_Y - LIST_Y0 - 8;
        float text_max = LIST_CARD_W - 86 - FAV_MARK - 18 - LIST_TEXT_INSET;
        px_marquee_begin();
        if (m->gsel != mq_gsel) {
            mq_gsel = m->gsel;
            if (!px_marquee_consume_seed()) px_marquee_reset();
        }
        for (i = 0; i < GAME_ROWS && m->gtop + i < m->ngames; i++) {
            int g = m->gtop + i, sel = g == m->gsel;
            float y = LIST_Y0 + i * LIST_ROW_PITCH;
            view_game_name(m->games[g], name, (int)sizeof name);
            px_card(c, 18, y, LIST_CARD_W, 54, sel, 0);
            game_image(c, m->tstate && m->tstate[g] == 1 ? &m->thumbs[g] : NULL, 32, y + 7, LIST_IMG, LIST_IMG, name,
                       sel);
            px_text_label(c, PX_MEDIUM, 20, 86, y + 14, name, sel ? px.on_pick : px.ink, 0, text_max, sel);
            if (game_is_favorite(m->games[g]))
                px_fav_mark(c, 18 + LIST_CARD_W - FAV_MARK - 6, y + 6, FAV_MARK);
            add_hit(c, 18, y, LIST_CARD_W, 54, HIT_ROW, g);
        }
        if (m->ngames > GAME_ROWS) {
            PxRect track, thumb;
            px_rail(c, LIST_RAIL_X, LIST_Y0, list_h, m->gtop, GAME_ROWS, m->ngames, &track, &thumb);
            g_hits[g_nhits].r = track;
            g_hits[g_nhits].id = HIT_VRAIL_TRACK;
            g_hits[g_nhits].arg = 0;
            g_nhits++;
            g_hits[g_nhits].r = thumb;
            g_hits[g_nhits].id = HIT_VRAIL_THUMB;
            g_hits[g_nhits].arg = 0;
            g_nhits++;
        }
    }
    library_bottom_faces(c, m);
}

static void row(PxCanvas *c, const Model *m, int i, float y)
{
    int sel = m->cursor == i && m->view == V_MAIN, dis = !view_row_enabled(m, i);
    uint32_t ink = sel && !dis ? px.on_pick : dis ? px.dim : px.ink;
    uint32_t sub = sel && !dis ? px.on_pick_sub : dis ? px.dim : px.muted;
    const char *title = i == ROW_ABOUT ? T("About") : T("Updates");
    px_card(c, 18, y, 588, 54, sel, dis);
    px_text_fit(c, PX_MEDIUM, 20, 38, y + 14, title, ink, 0, 330, PX_LEFT);
    if (i == ROW_ABOUT) {
        /* opens with A; nothing on the right */
    } else if (i == ROW_UPDATES) {
        char buf[64];
        int d;
        uint32_t v = upd_lit(&m->upd) || upd_running(&m->upd) ? (sel ? px.on_pick : px.ink)
                                                              : sel ? px.on_pick_sub : px.muted;
        upd_value(&m->upd, buf, sizeof buf, &d, m->dots);
        if (m->upd.state == UPD_DOWNLOADING) {
            /* the percentage in a box of its own (the word beside it stays put), and a bar along the card */
            char pct[8];
            int p = m->upd.pct < 0 ? 0 : m->upd.pct > 100 ? 100 : m->upd.pct;
            float pw = px_text_w(c, PX_MEDIUM, 20, "100%", 0);
            snprintf(pct, sizeof pct, "%d%%", p);
            px_text(c, PX_MEDIUM, 20, 584 - px_text_w(c, PX_MEDIUM, 20, pct, 0), y + 14, pct, v, 0);
            px_text_fit(c, PX_MEDIUM, 20, 584 - pw - 10, y + 14, T("Downloading"), v, 0, 240, PX_RIGHT);
            px_rect(c, 30, y + 46, 564, 3, sel ? px.on_pick_sub : px.line);
            px_rect(c, 30, y + 46, 564.0f * p / 100, 3, sel ? px.on_pick : px.accent.base);
        } else if (d >= 0) px_text_dots(c, PX_MEDIUM, 20, 584, y + 14, buf, d, v, 0, PX_RIGHT);
        else px_text_fit(c, PX_MEDIUM, 20, 584, y + 14, buf, v, 0, 300, PX_RIGHT);
    }
    (void)sub;
    add_hit(c, 18, y, 588, 54, HIT_ROW, i);
}

static void help(PxCanvas *c, const Model *m)
{
    char buf[320];
    const char *s;
    if (m->cursor == ROW_ABOUT)
        s = T("Local wireless multiplayer for DS games on BananaOS: two consoles on the same Wi-Fi play together, as on a real DS.");
    else {
        upd_help(&m->upd, "BananaDS LAN", buf, sizeof buf);
        s = buf;
    }
    px_text_wrap(c, PX_REGULAR, 18, 30, 84 + ROW_COUNT * 66 + 8, s, px.muted, 574, 4, 26);
}

/* a question over the list: a card in the middle, its two answers on the face row */
static void dialog(PxCanvas *c, const char *title, const char *sub, const char *mono)
{
    px_rect_a(c, 0, 0, 640, 480, 0x000000, px.light ? 70 : 150);
    px_card(c, 50, 120, 540, 200, 0, 0);
    px_text_wrap(c, PX_MEDIUM, 26, 76, 146, title, px.ink, 488, 2, 32);
    if (mono) px_text_fit(c, PX_REGULAR, 18, 76, 222, mono, px.muted, 0, 488, PX_LEFT);
    if (sub) px_text_wrap(c, PX_REGULAR, 18, 76, mono ? 252 : 222, sub, px.muted, 488, 3, 25);
}

/* SELECT: who made it, where its source is, what it is built on */
static void view_about(PxCanvas *c, const Model *m)
{
    char buf[96];
    PxRect r;
    float y;
    g_nhits = 0;
    px_screen(c);
    px_bubble(c, 18, 14, 604, 54, 0);
    px_text_fit(c, PX_MEDIUM, 26, 38, 25, T("About"), px.ink, 0, 440, PX_LEFT);
    px_card(c, 18, 82, 604, 326, 0, 0);
    px_text(c, PX_MEDIUM, 28, 40, 98, "BananaDS LAN", px.ink, 0);
    snprintf(buf, sizeof buf, "v%s", m->version[0] ? m->version : "?");
    px_text_fit(c, PX_MEDIUM, 20, 600, 104, buf, px.muted, 0, 160, PX_RIGHT);
    y = 138;
    y += 25 * px_text_wrap(c, PX_REGULAR, 18, 40, y,
                           T("Local wireless multiplayer for DS games on BananaOS: two consoles on the same Wi-Fi play together, as on a real DS."),
                           px.ink, 560, 3, 25) + 10;
    px_label(c, 40, y + 4, T("Made by"), px.muted);
    px_text(c, PX_MEDIUM, 18, 200, y, "Pixelado Dev", px.ink, 0);
    y += 26;
    px_label(c, 40, y + 4, T("Source code"), px.muted);
    px_text_fit(c, PX_MEDIUM, 18, 200, y, "github.com/pixeladodev/bananads-lan", px.ink, 0, 400, PX_LEFT);
    y += 32;
    px_label(c, 40, y, T("Credits"), px.muted);
    y += 20;
    y += 23 * px_text_wrap(c, PX_REGULAR, 16, 40, y,
                           T("BananaOS and BananaLinux by JorreFog and the Banana contributors (MIT). BananaDS (SuperDrastic) and melonDS (GPLv3). Pixelify Sans (OFL)."),
                           px.muted, 560, 3, 23) + 6;
    px_text_wrap(c, PX_REGULAR, 16, 40, y,
                 T("An independent app, not part of BananaOS. Built and tested on the RG DS and the RG DS Plus. Free software under the GPLv3."),
                 px.muted, 560, 3, 23);
    px_face(c, 18, 422, PX_LEFT, "B", T("Back"), 0, &r, 0, 0);
    add_face_hit(r, HIT_BACK);
}

void view_bottom(PxCanvas *c, const Model *m)
{
    char count[16];
    PxRect r;
    int i;
    if (m->view == V_GAMES) { games_bottom(c, m); return; }
    g_nhits = 0;
    px_screen(c);
    px_bubble(c, 18, 14, 604, 54, 0);
    px_text_fit(c, PX_MEDIUM, 26, 38, 25, T("Settings"), px.ink, 0, 440, PX_LEFT);
    snprintf(count, sizeof count, "%02d/%02d", m->cursor + 1, ROW_COUNT);
    px_text(c, PX_MEDIUM, 19, 602 - px_text_w(c, PX_MEDIUM, 19, count, 0), 30, count, px.muted, 0);
    for (i = 0; i < ROW_COUNT; i++) row(c, m, i, 84 + i * 66);
    px_rail(c, 615, 84, ROW_COUNT * 66 - 12, m->cursor, 1, ROW_COUNT, NULL, NULL);
    help(c, m);

    if (m->view == V_MAIN) {
        PxFace f[4];
        PxRect hr[4];
        int inst = upd_installing(&m->upd);
        f[0].key = "B"; f[0].label = T("Back"); f[0].dis = 0;
        f[1].key = "Y"; f[1].label = T("Uninstall"); f[1].dis = inst;
        f[2].key = "A"; f[2].label = m->cursor == ROW_UPDATES ? upd_action(&m->upd) : T("Open");
        f[2].dis = inst || (m->cursor == ROW_UPDATES && upd_running(&m->upd));
        px_faces(c, 422, 3, 2, f, hr);
        add_face_hit(hr[0], HIT_BACK);
        add_face_hit(hr[1], HIT_Y);
        add_face_hit(hr[2], HIT_OK);
        return;
    }
    if (m->view == V_ABOUT) {
        view_about(c, m);
        return;
    }
    g_nhits = 0;                     /* the list under the question takes no touch */
    if (m->view == V_UPDATE || m->view == V_UPDATED) {
        char title[96], sub[320];
        int done = m->view == V_UPDATED;
        snprintf(title, sizeof title, done ? T("Updated to v%s") : T("Update to v%s?"),
                 done ? m->upd.current : m->upd.latest);
        snprintf(sub, sizeof sub, "%s%s%s",
                 done ? T("A opens the new version.")
                      : T("It downloads from GitHub and checks it before anything changes. Your settings stay."),
                 done && m->upd.msg[0] ? " " : "", done ? m->upd.msg : "");
        dialog(c, title, sub, NULL);
        if (!done) {
            px_face(c, 18, 422, PX_LEFT, "B", T("Cancel"), 0, &r, 0, 0);
            add_face_hit(r, HIT_BACK);
        }
        px_face(c, 622, 422, PX_RIGHT, "A", done ? T("Restart") : T("Update"), 0, &r, 0, 0);
        add_face_hit(r, HIT_OK);
    } else if (m->view == V_UNINSTALL) {
        dialog(c, T("Uninstall BananaDS LAN?"), m->err[0] ? m->err : T("Turns it off and deletes this app."), NULL);
        px_face(c, 18, 422, PX_LEFT, "B", T("Cancel"), 0, &r, 0, 0);
        add_face_hit(r, HIT_BACK);
        px_face(c, 622, 422, PX_RIGHT, "A", T("Uninstall"), 0, &r, 0, 0);
        add_face_hit(r, HIT_OK);
    } else if (m->view == V_NEST) {
        dialog(c, T("One folder too deep."), m->err[0] ? m->err : T("A moves it up one folder."),
               "bananads-lan/bananads-lan");
        px_face(c, 18, 422, PX_LEFT, "B", T("Back"), 0, &r, 0, 0);
        add_face_hit(r, HIT_BACK);
        px_face(c, 622, 422, PX_RIGHT, "A", T("Move it"), 0, &r, 0, 0);
        add_face_hit(r, HIT_OK);
    } else {
        dialog(c, T("Moved."), T("Open BananaDS LAN again."), NULL);
        px_face(c, 622, 422, PX_RIGHT, "A", T("Exit"), 0, &r, 0, 0);
        add_face_hit(r, HIT_OK);
    }
}

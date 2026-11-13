/* BananaDS LAN: the two panels in the menu's look (pixel.c).
   Top: what the app sees (on or off, this BananaOS, this console, the network, the DS emulator, the version).
   Bottom: a list as Settings and Bshare draw theirs, with the keys along the bottom. */
#include "view.h"

#include <stdio.h>
#include <string.h>

Hit g_hits[MAX_HITS];
int g_nhits;

/* Spanish: neutral, and the words BananaOS's own translation uses (Encendido, Atrás, Consola...) */
const PxTr view_tr[] = {
    { "Local multiplayer", "Multijugador local" },
    { "Updates", "Actualizaciones" },
    { "Coming soon", "Próximamente" },
    { "Uninstall", "Desinstalar" },
    { "On", "Encendido" },
    { "Off", "Apagado" },
    { "Exit", "Salir" },
    { "Change", "Cambiar" },
    { "Cancel", "Cancelar" },
    { "Back", "Atrás" },
    { "Move it", "Moverla" },
    { "Ready", "Listo" },
    { "Not supported", "No compatible" },
    { "This console", "Esta consola" },
    { "Network", "Red" },
    { "DS emulator", "Emulador de DS" },
    { "App version", "Versión de la app" },
    { "No network", "Sin red" },
    { "Not BananaDS", "No es BananaDS" },
    { "Unknown", "Desconocida" },
    { "Needs %s", "Requiere %s" },
    { "Pick BananaDS in Settings > Nintendo DS > Emulator.",
      "Elige BananaDS en Ajustes > Nintendo DS > Emulador." },
    { "Open the same game on each console and go to its wireless mode: the consoles find each other.",
      "Abre el mismo juego en cada consola y entra a su modo inalámbrico: las consolas se encuentran solas." },
    { "Every DS game you open uses BananaDS with local multiplayer. It stays on after a reboot.",
      "Cada juego de DS que abras usa BananaDS con multijugador local. Sigue encendido al reiniciar." },
    { "Turned off, your games use the BananaDS that came with BananaOS.",
      "Apagado, tus juegos usan el BananaDS que vino con BananaOS." },
    { "Updates from inside the app are on the way. For now, get new versions from the GitHub releases.",
      "Las actualizaciones desde la app están en camino. Por ahora, descarga las versiones nuevas desde GitHub." },
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
    { NULL, NULL }
};

#define T(s) px_tr(s)

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
    return row != ROW_UPDATES;
}

static const char *model_name(const char *id)
{
    if (!strcmp(id, "rgds")) return "RG DS";
    if (!strcmp(id, "rgds_plus")) return "RG DS Plus";
    return T("Unknown");
}

static int is_bananads(const Model *m)
{
    return !strcmp(m->emulator, "bananads");
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
    int ready = m->supported && is_bananads(m);
    px_screen(c);
    px_status(c, "BananaDS LAN", m->battery, m->charging, m->clock);
    px_sheet(c, 18, 60, 604, 406);

    px_label(c, 44, 84, T("Local multiplayer"), px.ink);
    /* the state, big: green on, grey off */
    hw = px_badge(c, 42, 106, 52, hero, m->enabled ? PX_BADGE_ON : PX_BADGE_OFF);
    /* beside it: ready, or why not */
    snprintf(buf, sizeof buf, "%s", m->supported ? T("Ready") : T("Not supported"));
    cw = px_text_w(c, PX_MEDIUM, 15, buf, PX_UPPER | PX_TRACK) + 24;
    px_chip(c, 42 + hw + 18, 117, cw, 30, 0);
    px_text(c, PX_MEDIUM, 15, 42 + hw + 30, 122, buf, m->supported ? px.ink : px.warn, PX_UPPER | PX_TRACK);
    (void)ready;

    px_line_h(c, 42, 168, 556, 2, px.edge, 70);

    stat(c, 42, 186, 260, T("This console"), model_name(m->model), px.ink);
    if (m->os[0] && m->want_os[0] && strcmp(m->os, m->want_os)) {
        char need[64];
        snprintf(need, sizeof need, T("Needs %s"), m->want_os);
        snprintf(buf, sizeof buf, "%s  ·  %s", m->os, need);
        stat(c, 330, 186, 260, "BananaOS", buf, px.warn);
    } else stat(c, 330, 186, 260, "BananaOS", m->os[0] ? m->os : "?", px.ink);

    if (!m->ip[0]) snprintf(net, sizeof net, "%s", T("No network"));
    else if (!strncmp(m->ip, "10.42.0.", 8)) snprintf(net, sizeof net, "Direct Link  ·  %s", m->ip);
    else snprintf(net, sizeof net, "Wi-Fi  ·  %s", m->ip);
    stat(c, 42, 254, 260, T("Network"), net, m->ip[0] ? px.ink : px.muted);
    stat(c, 330, 254, 260, T("DS emulator"), is_bananads(m) ? "BananaDS" : T("Not BananaDS"),
         is_bananads(m) ? px.ink : px.warn);

    stat(c, 42, 322, 260, T("App version"), m->version[0] ? m->version : "?", px.ink);

    px_line_h(c, 42, 380, 556, 2, px.edge, 70);
    if (m->err[0]) { msg = m->err; msg_rgb = px.warn; }
    else if (!m->supported && m->reason[0]) { msg = m->reason; msg_rgb = px.warn; }
    else if (!is_bananads(m)) msg = T("Pick BananaDS in Settings > Nintendo DS > Emulator.");
    else msg = T("Open the same game on each console and go to its wireless mode: the consoles find each other.");
    px_text_wrap(c, PX_REGULAR, 18, 42, 392, msg, msg_rgb, 556, 2, 24);
}

static void row(PxCanvas *c, const Model *m, int i, float y)
{
    int sel = m->cursor == i && m->view == V_MAIN, dis = !view_row_enabled(m, i);
    uint32_t ink = sel && !dis ? px.on_pick : dis ? px.dim : px.ink;
    uint32_t sub = sel && !dis ? px.on_pick_sub : dis ? px.dim : px.muted;
    const char *title = i == ROW_MP ? T("Local multiplayer") : T("Updates");
    px_card(c, 18, y, 588, 54, sel, dis);
    px_text_fit(c, PX_MEDIUM, 20, 38, y + 14, title, ink, 0, 330, PX_LEFT);
    if (i == ROW_MP)
        px_value_arrows(c, 584, y + 14, 20, m->enabled ? T("On") : T("Off"), sel ? ink : px.ink, 1, 1);
    else if (i == ROW_UPDATES)
        px_text_fit(c, PX_REGULAR, 18, 584, y + 16, T("Coming soon"), sub, 0, 220, PX_RIGHT);
    add_hit(c, 18, y, 588, 54, HIT_ROW, i);
}

static void help(PxCanvas *c, const Model *m)
{
    const char *s;
    if (m->cursor == ROW_MP)
        s = m->enabled ? T("Every DS game you open uses BananaDS with local multiplayer. It stays on after a reboot.")
                       : T("Turned off, your games use the BananaDS that came with BananaOS.");
    else
        s = T("Updates from inside the app are on the way. For now, get new versions from the GitHub releases.");
    px_text_wrap(c, PX_REGULAR, 18, 30, 232, s, px.muted, 574, 5, 26);
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
    px_face(c, 18, 422, PX_LEFT, "B", T("Back"), 0, &r);
    add_face_hit(r, HIT_BACK);
}

void view_bottom(PxCanvas *c, const Model *m)
{
    char count[16];
    PxRect r;
    int i;
    g_nhits = 0;
    px_screen(c);
    px_bubble(c, 18, 14, 604, 54, 0);
    px_text_fit(c, PX_MEDIUM, 26, 38, 25, "BananaDS LAN", px.ink, 0, 440, PX_LEFT);
    snprintf(count, sizeof count, "%02d/%02d", m->cursor + 1, ROW_COUNT);
    px_text(c, PX_MEDIUM, 19, 602 - px_text_w(c, PX_MEDIUM, 19, count, 0), 30, count, px.muted, 0);
    for (i = 0; i < ROW_COUNT; i++) row(c, m, i, 84 + i * 66);
    px_rail(c, 615, 84, 120, m->cursor, 1, ROW_COUNT);
    help(c, m);

    if (m->view == V_MAIN) {
        PxFace f[4];
        PxRect hr[4];
        f[0].key = "B"; f[0].label = T("Exit"); f[0].dis = 0;
        f[1].key = "SELECT"; f[1].label = T("About"); f[1].dis = 0;
        f[2].key = "Y"; f[2].label = T("Uninstall"); f[2].dis = 0;
        f[3].key = "A"; f[3].label = T("Change"); f[3].dis = !view_row_enabled(m, m->cursor);
        px_faces(c, 422, 4, 2, f, hr);
        add_face_hit(hr[0], HIT_BACK);
        add_face_hit(hr[1], HIT_SELECT);
        add_face_hit(hr[2], HIT_Y);
        add_face_hit(hr[3], HIT_OK);
        return;
    }
    if (m->view == V_ABOUT) {
        view_about(c, m);
        return;
    }
    g_nhits = 0;                     /* the list under the question takes no touch */
    if (m->view == V_UNINSTALL) {
        dialog(c, T("Uninstall BananaDS LAN?"), m->err[0] ? m->err : T("Turns it off and deletes this app."), NULL);
        px_face(c, 18, 422, PX_LEFT, "B", T("Cancel"), 0, &r);
        add_face_hit(r, HIT_BACK);
        px_face(c, 622, 422, PX_RIGHT, "A", T("Uninstall"), 0, &r);
        add_face_hit(r, HIT_OK);
    } else if (m->view == V_NEST) {
        dialog(c, T("One folder too deep."), m->err[0] ? m->err : T("A moves it up one folder."),
               "bananads-lan/bananads-lan");
        px_face(c, 18, 422, PX_LEFT, "B", T("Back"), 0, &r);
        add_face_hit(r, HIT_BACK);
        px_face(c, 622, 422, PX_RIGHT, "A", T("Move it"), 0, &r);
        add_face_hit(r, HIT_OK);
    } else {
        dialog(c, T("Moved."), T("Open BananaDS LAN again."), NULL);
        px_face(c, 622, 422, PX_RIGHT, "A", T("Exit"), 0, &r);
        add_face_hit(r, HIT_OK);
    }
}

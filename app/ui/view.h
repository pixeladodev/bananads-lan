/* BananaDS LAN's two panels, drawn from a model (main.c fills it on the handheld, preview.c on a computer). */
#ifndef VIEW_H
#define VIEW_H

#include "pixel.h"

enum { V_MAIN, V_UNINSTALL, V_NEST, V_NEST_OK };
enum { ROW_MP, ROW_UPDATES, ROW_UNINSTALL, ROW_COUNT };
/* what a touch on the bottom panel does */
enum { HIT_NONE, HIT_ROW, HIT_BACK, HIT_OK, HIT_Y };

typedef struct {
    int supported, enabled, mounted;
    char reason[240], emulator[64], os[32], want_os[32], version[32], model[32];
    char ip[48];                 /* wlan0's IPv4 address, empty without one */
    char err[240];               /* the last script's complaint */
    int view, cursor;
    int battery, charging;
    char clock[16];
} Model;

typedef struct { PxRect r; int id, arg; } Hit;

#define MAX_HITS 12
extern Hit g_hits[MAX_HITS];
extern int g_nhits;

extern const PxTr view_tr[];
void view_top(PxCanvas *c, const Model *m);
void view_bottom(PxCanvas *c, const Model *m);
int view_row_enabled(const Model *m, int row);

#endif

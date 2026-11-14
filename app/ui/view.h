/* BananaDS LAN's two panels, drawn from a model (main.c fills it on the handheld, preview.c on a computer). */
#ifndef VIEW_H
#define VIEW_H

#include "pixel.h"
#include "pixel_image.h"
#include "updater.h"

#include <time.h>

/* V_GAMES, the library, is where the app opens; V_MAIN is its settings (START) */
enum { V_MAIN, V_UNINSTALL, V_NEST, V_NEST_OK, V_ABOUT, V_UPDATE, V_UPDATED, V_GAMES };
enum { ROW_MP, ROW_UPDATES, ROW_COUNT };
/* the library as a list: rows on the bottom panel at once */
#define GAME_ROWS 5
/* the library as a grid (the default): GRID_ROWS tiles a column, GRID_COLS columns in view, scrolled sideways */
#define GRID_ROWS 2
#define GRID_COLS 3
#define TILE_W 194.0f
#define TILE_H 156.0f
#define TILE_GAP 11.0f
#define TILE_IMG_W 178.0f
#define TILE_IMG_H 110.0f
#define LIST_IMG 40.0f
/* the cover on the top panel */
#define ART_W 220.0f
#define ART_H 260.0f
#define MAX_GAMES 2048
#define GAME_PATH 384
/* what a touch on the bottom panel does */
enum { HIT_NONE, HIT_ROW, HIT_BACK, HIT_OK, HIT_Y, HIT_SELECT, HIT_X, HIT_START };

typedef struct {
    int supported, enabled, mounted;
    char reason[240], os[32], version[32], model[32];
    char ip[48];                 /* wlan0's IPv4 address, empty without one */
    char err[240];               /* the last script's complaint */
    int view, cursor;
    PxUpdate upd;                /* the Updates row: update.py's last word */
    int dots;                    /* the animated dots, 1-3 */
    int battery, charging;
    char clock[16];
    /* the DS games (V_GAMES), from the system's NDS folders, played on the app's own BananaDS */
    char (*games)[GAME_PATH];
    int ngames, gsel, gtop;
    int grid, gleft;             /* the grid (1) or the list; the grid's first column in view */
    PxImage *thumbs;             /* a game's cover or icon at tile size, read as it comes into view */
    signed char *tstate;         /* 0 not read yet, 1 read, -1 none */
    PxImage art;                 /* the picked game's cover (or icon) at the top panel's size */
    int art_game;                /* whose (-1: none read) */
    /* the picked game: its save (the one both BananaDS share), and play time as the menu counts it plus the app's */
    long gsave;                  /* in bytes, -1 without one */
    time_t gsave_time;
    long gsecs;
    int gplays;
    time_t glast;
    int clock12;                 /* settings.conf's clock = 12h */
    int emu_ok;                  /* the app's own BananaDS (emu/bananads) is there */
    int from_games;              /* About opened from the library: B goes back there */
} Model;

typedef struct { PxRect r; int id, arg; } Hit;

#define MAX_HITS 16
extern Hit g_hits[MAX_HITS];
extern int g_nhits;

extern const PxTr view_tr[];
void view_top(PxCanvas *c, const Model *m);
void view_bottom(PxCanvas *c, const Model *m);
int view_row_enabled(const Model *m, int row);
/* a game's name as the list shows it: its file name without the folder and the extension */
void view_game_name(const char *path, char *dst, int n);

#endif

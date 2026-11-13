/* The app's updater on screen: update.py (beside the app) run in the background, its state file read back.
   Shared by BananaDS LAN and Direct Link. */
#ifndef UPDATER_H
#define UPDATER_H

#include <stddef.h>
#include <sys/types.h>

#include "pixel.h"

enum {
    UPD_IDLE,         /* not looked yet */
    UPD_CHECKING,
    UPD_UPTODATE,
    UPD_AVAILABLE,
    UPD_OFFLINE,      /* no network */
    UPD_FAILED,       /* msg says why; nothing was changed */
    UPD_DOWNLOADING,  /* pct */
    UPD_VERIFYING,
    UPD_INSTALLING,
    UPD_DONE          /* the new version is in place: the app opens it again */
};

typedef struct {
    int state, pct, installing;   /* installing: the job was an install (a check otherwise) */
    char current[32], latest[32], msg[160];
    pid_t pid;
} PxUpdate;

/* the exit code that asks tile.sh to open the app again (the new version) */
#define UPD_RESTART 75

extern const PxTr upd_tr[];

/* starts update.py check (install = 0) or install; nothing when one runs already */
void upd_start(PxUpdate *u, const char *app_dir, const char *id, int install);
/* reads the state back; 1 when it changed */
int upd_poll(PxUpdate *u);
int upd_running(const PxUpdate *u);
/* while it downloads, checks or installs: the app stays open */
int upd_installing(const PxUpdate *u);

/* the Updates row's value; *dots >= 0 when it ends in the animated dots */
void upd_value(const PxUpdate *u, char *buf, size_t n, int *dots, int frame);
/* a sentence about it, for the help area */
void upd_help(const PxUpdate *u, const char *app_name, char *buf, size_t n);
/* A on the Updates row */
const char *upd_action(const PxUpdate *u);
/* the colour of the value: the accent when there is something to install */
int upd_lit(const PxUpdate *u);

#endif

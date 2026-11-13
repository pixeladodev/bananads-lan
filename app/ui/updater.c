/* The app's updater on screen (updater.h). update.py does the work; this starts it, reads its state file and
   words what it says. */
#include "updater.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

const PxTr upd_tr[] = {
    { "Updates", "Actualizaciones" },
    { "Checking", "Buscando" },
    { "Up to date", "Al día" },
    { "v%s available", "v%s disponible" },
    { "No network", "Sin red" },
    { "Didn't work", "No funcionó" },
    { "Downloading %d%%", "Descargando %d%%" },
    { "Downloading", "Descargando" },
    { "Verifying", "Verificando" },
    { "Installing", "Instalando" },
    { "Updated", "Actualizada" },
    { "Check", "Buscar" },
    { "Update", "Actualizar" },
    { "Restart", "Reiniciar" },
    { "Looking for a newer version on GitHub", "Buscando una versión más nueva en GitHub" },
    { "%s %s is the newest version. A looks again.", "%s %s es la versión más reciente. A vuelve a buscar." },
    { "Version %s is out (you have %s). A downloads and installs it; your settings stay.",
      "Salió la versión %s (tienes la %s). A la descarga e instala; tus ajustes se mantienen." },
    { "GitHub can't be reached. Connect to Wi-Fi and press A to look again.",
      "No se puede llegar a GitHub. Conéctate al Wi-Fi y presiona A para buscar de nuevo." },
    { "The update didn't work: %s. Nothing was changed; A tries again.",
      "La actualización no funcionó: %s. No se cambió nada; A lo intenta de nuevo." },
    { "Couldn't look for updates: %s. A tries again.", "No se pudo buscar actualizaciones: %s. A lo intenta de nuevo." },
    { "Downloading, checking and installing it. Don't turn off the console; it takes under a minute.",
      "Descargándola, verificándola e instalándola. No apagues la consola; toma menos de un minuto." },
    { "A looks for a newer version on GitHub.", "A busca una versión más nueva en GitHub." },
    { "Update to v%s?", "¿Actualizar a la v%s?" },
    { "It downloads from GitHub and checks it before anything changes. Your settings stay.",
      "Se descarga de GitHub y se verifica antes de cambiar nada. Tus ajustes se mantienen." },
    { "Updated to v%s", "Actualizada a la v%s" },
    { "A opens the new version.", "A abre la versión nueva." },
    /* update.py's reasons */
    { "No release yet", "aún no hay versiones" },
    { "GitHub is busy, try later", "GitHub está ocupado, prueba más tarde" },
    { "Bad answer from GitHub", "GitHub respondió algo inesperado" },
    { "Not for this console yet", "aún no es para esta consola" },
    { "Download failed", "falló la descarga" },
    { "Download damaged", "la descarga llegó dañada" },
    { "No checksum published for it", "no tiene suma de verificación publicada" },
    { "Package damaged", "el paquete está dañado" },
    { "Package incomplete", "el paquete está incompleto" },
    { "Wrong version inside", "el paquete trae otra versión" },
    { "Not enough space", "no hay espacio suficiente" },
    { "Could not install it", "no se pudo instalar" },
    { "Disconnect first", "desconecta primero" },
    { "Not for this BananaOS", "no es para este BananaOS" },
    { "Needs Python 3", "requiere Python 3" },
    { "The updater stopped", "el actualizador se detuvo" },
    { NULL, NULL }
};

#define T(s) px_tr(s)

static char g_state_path[256];

static void copy_str(char *dst, size_t n, const char *src)
{
    size_t i = 0;
    if (!n) return;
    while (i + 1 < n && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

void upd_start(PxUpdate *u, const char *app_dir, const char *id, int install)
{
    char script[600];
    pid_t pid;
    if (u->pid > 0) return;
    snprintf(g_state_path, sizeof g_state_path, "/tmp/%s-update.state", id);
    unlink(g_state_path);
    snprintf(script, sizeof script, "%s/update.py", app_dir);
    u->installing = install;
    u->state = install ? UPD_DOWNLOADING : UPD_CHECKING;
    u->pct = 0;
    u->msg[0] = 0;
    pid = fork();
    if (pid < 0) {
        u->state = UPD_FAILED;
        copy_str(u->msg, sizeof u->msg, "The updater stopped");
        return;
    }
    if (pid == 0) {
        int fd = open("/dev/null", O_RDWR);
        if (fd >= 0) { dup2(fd, 0); dup2(fd, 1); dup2(fd, 2); if (fd > 2) close(fd); }
        setenv("PIXEL_UPDATE_STATE", g_state_path, 1);
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        execlp("python3", "python3", script, install ? "install" : "check", (char *)NULL);
        _exit(127);
    }
    u->pid = pid;
}

static int state_of(const char *s)
{
    static const char *const names[] = { "", "checking", "uptodate", "available", "offline", "failed",
                                         "downloading", "verifying", "installing", "done" };
    int i;
    for (i = 1; i < (int)(sizeof names / sizeof names[0]); i++)
        if (!strcmp(s, names[i])) return i;
    return -1;
}

static void read_state(PxUpdate *u)
{
    char line[256];
    FILE *f;
    if (!g_state_path[0] || !(f = fopen(g_state_path, "r"))) return;
    while (fgets(line, sizeof line, f)) {
        char *nl = strchr(line, '\n'), *v = strchr(line, '=');
        if (nl) *nl = 0;
        if (!v) continue;
        *v++ = 0;
        if (!strcmp(line, "state")) {
            int s = state_of(v);
            if (s > 0) u->state = s;
        } else if (!strcmp(line, "pct")) u->pct = atoi(v);
        else if (!strcmp(line, "current") && v[0]) copy_str(u->current, sizeof u->current, v);
        else if (!strcmp(line, "latest")) copy_str(u->latest, sizeof u->latest, v);
        else if (!strcmp(line, "msg")) copy_str(u->msg, sizeof u->msg, v);
    }
    fclose(f);
}

int upd_poll(PxUpdate *u)
{
    PxUpdate was = *u;
    int st = 0;
    pid_t p, pid = u->pid;
    if (pid <= 0) return 0;
    read_state(u);
    p = waitpid(pid, &st, WNOHANG);
    if (p == pid || (p < 0 && errno == ECHILD)) {
        u->pid = 0;
        read_state(u);
        if (p == pid && WIFEXITED(st) && WEXITSTATUS(st) == 127 && u->state != UPD_DONE) {
            u->state = UPD_FAILED;
            copy_str(u->msg, sizeof u->msg, "Needs Python 3");
        } else if (u->state == UPD_CHECKING || u->state == UPD_DOWNLOADING || u->state == UPD_VERIFYING ||
                   u->state == UPD_INSTALLING) {
            u->state = UPD_FAILED;
            if (!u->msg[0]) copy_str(u->msg, sizeof u->msg, "The updater stopped");
        }
    }
    return memcmp(&was, u, sizeof was) != 0;
}

int upd_running(const PxUpdate *u)
{
    return u->pid > 0;
}

int upd_installing(const PxUpdate *u)
{
    return u->pid > 0 && u->installing;
}

void upd_value(const PxUpdate *u, char *buf, size_t n, int *dots, int frame)
{
    *dots = -1;
    switch (u->state) {
    case UPD_CHECKING: snprintf(buf, n, "%s", T("Checking")); *dots = frame; break;
    case UPD_UPTODATE: snprintf(buf, n, "%s", T("Up to date")); break;
    case UPD_AVAILABLE: snprintf(buf, n, T("v%s available"), u->latest); break;
    case UPD_OFFLINE: snprintf(buf, n, "%s", T("No network")); break;
    case UPD_FAILED: snprintf(buf, n, "%s", T("Didn't work")); break;
    case UPD_DOWNLOADING: snprintf(buf, n, T("Downloading %d%%"), u->pct); break;
    case UPD_VERIFYING: snprintf(buf, n, "%s", T("Verifying")); *dots = frame; break;
    case UPD_INSTALLING: snprintf(buf, n, "%s", T("Installing")); *dots = frame; break;
    case UPD_DONE: snprintf(buf, n, "%s", T("Updated")); break;
    default: snprintf(buf, n, "-"); break;
    }
}

void upd_help(const PxUpdate *u, const char *app_name, char *buf, size_t n)
{
    const char *why = u->msg[0] ? T(u->msg) : T("Didn't work");
    switch (u->state) {
    case UPD_CHECKING: snprintf(buf, n, "%s...", T("Looking for a newer version on GitHub")); break;
    case UPD_UPTODATE: snprintf(buf, n, T("%s %s is the newest version. A looks again."), app_name, u->current); break;
    case UPD_AVAILABLE:
        snprintf(buf, n, T("Version %s is out (you have %s). A downloads and installs it; your settings stay."),
                 u->latest, u->current);
        break;
    case UPD_OFFLINE: snprintf(buf, n, "%s", T("GitHub can't be reached. Connect to Wi-Fi and press A to look again.")); break;
    case UPD_FAILED:
        snprintf(buf, n, u->installing ? T("The update didn't work: %s. Nothing was changed; A tries again.")
                                       : T("Couldn't look for updates: %s. A tries again."), why);
        break;
    case UPD_DOWNLOADING: case UPD_VERIFYING: case UPD_INSTALLING:
        snprintf(buf, n, "%s", T("Downloading, checking and installing it. Don't turn off the console; it takes under a minute."));
        break;
    case UPD_DONE: snprintf(buf, n, T("Updated to v%s"), u->current); break;
    default: snprintf(buf, n, "%s", T("A looks for a newer version on GitHub.")); break;
    }
}

const char *upd_action(const PxUpdate *u)
{
    return u->state == UPD_AVAILABLE ? T("Update") : T("Check");
}

int upd_lit(const PxUpdate *u)
{
    return u->state == UPD_AVAILABLE || u->state == UPD_DONE;
}

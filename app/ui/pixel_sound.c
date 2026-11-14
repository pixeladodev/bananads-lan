/* pixel_sound.h: the menu's sounds through ALSA, opened with dlopen */
#include "pixel_sound.h"

#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

typedef struct snd_pcm_ snd_pcm_t;
/* (alsa/asoundlib.h's values: SND_PCM_STREAM_PLAYBACK, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED) */
enum { PCM_PLAYBACK = 0, PCM_S16_LE = 2, PCM_RW_INTERLEAVED = 3 };
static int (*a_open)(snd_pcm_t **, const char *, int, int);
static int (*a_set_params)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned);
static long (*a_writei)(snd_pcm_t *, const void *, unsigned long);
static int (*a_recover)(snd_pcm_t *, int, int);
static int (*a_drop)(snd_pcm_t *);
static int (*a_close)(snd_pcm_t *);
static int (*a_cfg_free)(void);

#define RATE 48000
#define CH 2
#define BLOCK (RATE / 100)          /* 10 ms */
#define QUIET_MS 20000

typedef struct { const char *name; int16_t *pcm; size_t frames; } Clip;
static Clip g_clips[] = { { "scroll", 0, 0 }, { "select", 0, 0 }, { "back", 0, 0 }, { "ready", 0, 0 } };
#define NCLIPS ((int)(sizeof g_clips / sizeof g_clips[0]))

static int g_on;                    /* sounds = on and libasound found */
static pthread_t g_th;
static int g_started;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv = PTHREAD_COND_INITIALIZER;
static int g_queued = -1, g_stop, g_close, g_closed = 1;

static long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static uint32_t le(const uint8_t *p, int n)
{
    uint32_t v = 0;
    int i;
    for (i = n - 1; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}

/* a 16-bit PCM WAV, as 48 kHz stereo (banana-ui converts each clip once too: the RG DS Plus's amplifier is a
   48 kHz part, and no plug resampler is then involved) */
static int load_wav(const char *path, Clip *c)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    long len;
    size_t at = 12;
    int ch = 0, bits = 0;
    uint32_t rate = 0;
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) || (len = ftell(f)) < 44 || len > 8 * 1024 * 1024 || fseek(f, 0, SEEK_SET)) { fclose(f); return 0; }
    b = malloc((size_t)len);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) { fclose(f); free(b); return 0; }
    fclose(f);
    if (memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) { free(b); return 0; }
    while (at + 8 <= (size_t)len) {
        uint32_t n = le(b + at + 4, 4);
        const uint8_t *d = b + at + 8;
        if (n > (size_t)len - at - 8) n = (uint32_t)((size_t)len - at - 8);
        if (!memcmp(b + at, "fmt ", 4) && n >= 16) {
            ch = (int)le(d + 2, 2);
            rate = le(d + 4, 4);
            bits = (int)le(d + 14, 2);
        } else if (!memcmp(b + at, "data", 4)) {
            size_t in, out, i;
            const int16_t *src = (const int16_t *)d;
            if (bits != 16 || ch < 1 || ch > 8 || rate < 8000 || rate > 192000) break;
            in = n / 2 / (size_t)ch;
            if (in < 2) break;
            out = (size_t)((double)in * RATE / rate);
            c->pcm = malloc(out * CH * sizeof(int16_t));
            if (!c->pcm) break;
            for (i = 0; i < out; i++) {
                double x = (double)i * rate / RATE, t;
                size_t j = (size_t)x;
                int k;
                if (j + 1 >= in) j = in - 2;
                t = x - (double)j;
                if (t > 1) t = 1;
                for (k = 0; k < CH; k++) {
                    int sc = k < ch ? k : 0;    /* (mono to both sides) */
                    double a = src[j * (size_t)ch + (size_t)sc], bb = src[(j + 1) * (size_t)ch + (size_t)sc];
                    c->pcm[i * CH + (size_t)k] = (int16_t)(a + (bb - a) * t);
                }
            }
            c->frames = out;
            free(b);
            return 1;
        }
        at += 8 + n + (n & 1);
    }
    free(b);
    return 0;
}

/* bananad's sound route: replaced when the jack changes, and alsa-lib reads its configuration once per process */
static long long route_id(void)
{
    struct stat st;
    if (stat("/run/banana/asound.conf", &st)) return 0;
    return (long long)st.st_ino * 1000003LL + (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
}

/* the device is closed (or never opened): px_sound_close stops waiting */
static void mark_closed(void)
{
    pthread_mutex_lock(&g_mu);
    g_closed = 1;
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mu);
}

static void *run(void *arg)
{
    snd_pcm_t *pcm = NULL;
    static int16_t silence[BLOCK * CH];
    const Clip *cur = NULL;
    size_t at = 0;
    long last = now_ms();
    long long route = route_id();
    int check = 0;
    (void)arg;
    for (;;) {
        int q;
        pthread_mutex_lock(&g_mu);
        while (!pcm && !g_stop && g_queued < 0 && !g_close) pthread_cond_wait(&g_cv, &g_mu);
        if (g_stop) { pthread_mutex_unlock(&g_mu); break; }
        q = g_queued;
        g_queued = -1;
        if (g_close && !cur && q < 0) {
            if (pcm) { a_drop(pcm); a_close(pcm); pcm = NULL; }
            g_close = 0;
            g_closed = 1;
            pthread_cond_broadcast(&g_cv);
            pthread_mutex_unlock(&g_mu);
            continue;
        }
        pthread_mutex_unlock(&g_mu);
        if (q >= 0) {
            if (!pcm) {
                long long r = route_id();
                if (r != route && a_cfg_free) { a_cfg_free(); route = r; }
                if (a_open(&pcm, "default", PCM_PLAYBACK, 0) < 0) { pcm = NULL; mark_closed(); continue; }
                if (a_set_params(pcm, PCM_S16_LE, PCM_RW_INTERLEAVED, CH, RATE, 1, 30000) < 0) {
                    a_close(pcm);
                    pcm = NULL;
                    mark_closed();
                    continue;
                }
            }
            cur = &g_clips[q];
            at = 0;
            last = now_ms();
        } else if (!cur && now_ms() - last >= QUIET_MS) {
            pthread_mutex_lock(&g_mu);
            if (pcm) { a_drop(pcm); a_close(pcm); pcm = NULL; }
            g_closed = 1;
            pthread_cond_broadcast(&g_cv);
            pthread_mutex_unlock(&g_mu);
            continue;
        }
        if (!pcm) continue;
        if (++check >= 25) {
            check = 0;
            if (route_id() != route) { a_drop(pcm); a_close(pcm); pcm = NULL; cur = NULL; mark_closed(); continue; }
        }
        {
            const int16_t *p = silence;
            long frames = BLOCK;
            if (cur) {
                size_t left = cur->frames - at;
                if ((size_t)frames > left) frames = (long)left;
                p = cur->pcm + at * CH;
                at += (size_t)frames;
                if (at >= cur->frames) { cur = NULL; last = now_ms(); }
            }
            while (frames > 0) {
                long w = a_writei(pcm, p, (unsigned long)frames);   /* blocks while the 30 ms buffer is full */
                if (w < 0) {
                    if (a_recover(pcm, (int)w, 1) < 0) { a_close(pcm); pcm = NULL; cur = NULL; mark_closed(); break; }
                    continue;
                }
                frames -= w;
                p += w * CH;
            }
        }
    }
    if (pcm) { a_drop(pcm); a_close(pcm); }
    return NULL;
}

void px_sound_init(const PxSettings *st, const char *dir)
{
    void *lib;
    int i, any = 0;
    if (st && !strcmp(st->sounds, "off")) return;
    if (!dir) dir = getenv("PIXEL_UI_SOUNDS");
    if (!dir || !dir[0]) dir = "/usr/share/banana/rnds/sounds";
    lib = dlopen("libasound.so.2", RTLD_NOW | RTLD_LOCAL);
    if (!lib) return;
    a_open = (int (*)(snd_pcm_t **, const char *, int, int))dlsym(lib, "snd_pcm_open");
    a_set_params = (int (*)(snd_pcm_t *, int, int, unsigned, unsigned, int, unsigned))dlsym(lib, "snd_pcm_set_params");
    a_writei = (long (*)(snd_pcm_t *, const void *, unsigned long))dlsym(lib, "snd_pcm_writei");
    a_recover = (int (*)(snd_pcm_t *, int, int))dlsym(lib, "snd_pcm_recover");
    a_drop = (int (*)(snd_pcm_t *))dlsym(lib, "snd_pcm_drop");
    a_close = (int (*)(snd_pcm_t *))dlsym(lib, "snd_pcm_close");
    a_cfg_free = (int (*)(void))dlsym(lib, "snd_config_update_free_global");
    if (!a_open || !a_set_params || !a_writei || !a_recover || !a_drop || !a_close) return;
    for (i = 0; i < NCLIPS; i++) {
        char path[640];
        /* the user theme's own sound for it ("off": none), else the theme's own (one that doesn't load: the theme's) */
        int t = px_theme_sound(st, g_clips[i].name, path, sizeof path);
        if (t < 0) continue;
        if (t > 0 && load_wav(path, &g_clips[i])) { any = 1; continue; }
        snprintf(path, sizeof path, "%s/%s.wav", dir, g_clips[i].name);
        if (load_wav(path, &g_clips[i])) any = 1;
    }
    g_on = any;
}

void px_sound_play(const char *name)
{
    int i;
    if (!g_on) return;
    for (i = 0; i < NCLIPS; i++)
        if (!strcmp(g_clips[i].name, name)) break;
    if (i == NCLIPS || !g_clips[i].pcm) return;
    pthread_mutex_lock(&g_mu);
    if (!g_started) g_started = pthread_create(&g_th, NULL, run, NULL) == 0;
    g_queued = i;
    g_closed = 0;                   /* (busy from now: a close waits for this sound) */
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mu);
}

void px_sound_close(int max_ms)
{
    struct timespec until;
    if (!g_on || !g_started) return;
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += max_ms / 1000;
    until.tv_nsec += (long)(max_ms % 1000) * 1000000L;
    if (until.tv_nsec >= 1000000000L) { until.tv_sec++; until.tv_nsec -= 1000000000L; }
    pthread_mutex_lock(&g_mu);
    g_close = 1;
    pthread_cond_broadcast(&g_cv);
    while (!g_closed) if (pthread_cond_timedwait(&g_cv, &g_mu, &until)) break;
    g_stop = 1;
    pthread_cond_broadcast(&g_cv);
    pthread_mutex_unlock(&g_mu);
    pthread_join(g_th, NULL);
    g_started = 0;
}

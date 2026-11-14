/* The menu's sounds in an app, as banana-ui plays them (its sound.cpp): the theme's rnds/sounds/<name>.wav (or the
   user theme's own, or none when it turns one off), through ALSA's "default" (bananad's /run/banana/asound.conf: the
   speakers or the headphones, at the system volume). Nothing plays when settings.conf says sounds = off.
   libasound is opened at run time (dlopen): without it the app is silent, never broken.
   The device is opened at the first sound and kept running (silence between sounds, so the RG DS Plus's speaker
   amplifiers stay started) until 20 quiet seconds have passed or px_sound_close. A new sound cuts the one playing
   (a held direction scrolls fast). */
#ifndef PIXEL_SOUND_H
#define PIXEL_SOUND_H

#include "pixel.h"

/* the clips (scroll, select, back, ready) and the setting; dir NULL is /usr/share/banana/rnds/sounds
   (PIXEL_UI_SOUNDS overrides it) */
void px_sound_init(const PxSettings *st, const char *dir);
/* play one (scroll, select, back, ready); nothing when sounds are off or the clip is missing */
void px_sound_play(const char *name);
/* let what is playing finish (at most max_ms), then close the device: before the app quits or a game starts */
void px_sound_close(int max_ms);

#endif

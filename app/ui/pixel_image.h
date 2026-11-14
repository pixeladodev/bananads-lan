/* Pictures in an app: a game's scraped cover (media/box/<rom name>.png or .jpg beside the ROMs, as banana-ui finds
   it) or the icon a DS ROM carries in its banner, decoded once and drawn into a box. */
#ifndef PIXEL_IMAGE_H
#define PIXEL_IMAGE_H

#include "pixel.h"

typedef struct {
    uint32_t *px;               /* ARGB8888, w * h; alpha 0 is see-through (an icon's colour 0) */
    int w, h;
    int pixel_art;              /* drawn bigger by whole steps, never smoothed (the 32x32 DS icon) */
} PxImage;

/* a PNG or JPEG, shrunk to fit max_w x max_h (pixels; never grown); 0 when it can't be read */
int px_image_load(const char *path, int max_w, int max_h, PxImage *img);
/* the 32x32 icon in a .nds/.dsi ROM's banner; 0 when the ROM has none (or is zipped) */
int px_nds_icon(const char *rom, PxImage *img);
/* the ROM banner's title in a language ("es", "en", ...: English when that one is empty), one line (its lines
   joined by " - "); 0 when there is none */
int px_nds_title(const char *rom, const char *lang, char *out, int outsz);
void px_image_free(PxImage *img);
/* centred in the box (units), as big as fits without cropping */
void px_image_draw(PxCanvas *c, float x, float y, float w, float h, const PxImage *img);
/* the cover for a ROM: <rom dir>/media/box/<rom name without extension>.png or .jpg (case as exFAT has it: any);
   0 when there is none */
int px_find_box(const char *rom, char *out, int outsz);

#endif

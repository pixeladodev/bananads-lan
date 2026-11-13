/* pixel_png: a PNG file from XRGB8888 pixels, for previews on a computer (no zlib: stored deflate blocks). */
#ifndef PIXEL_PNG_H
#define PIXEL_PNG_H
#include <stdint.h>
int px_png_write(const char *path, const uint32_t *px, int w, int h);
#endif

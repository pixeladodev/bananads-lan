/* pixel_png: see pixel_png.h. Uncompressed, so the files are big; they are previews. */
#include "pixel_png.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t crc_table[256];

static uint32_t crc(uint32_t c, const unsigned char *b, size_t n)
{
    size_t i;
    if (!crc_table[1]) {
        uint32_t k, j, v;
        for (k = 0; k < 256; k++) {
            v = k;
            for (j = 0; j < 8; j++) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1;
            crc_table[k] = v;
        }
    }
    for (i = 0; i < n; i++) c = crc_table[(c ^ b[i]) & 255] ^ (c >> 8);
    return c;
}

static void be32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24); p[1] = (unsigned char)(v >> 16); p[2] = (unsigned char)(v >> 8); p[3] = (unsigned char)v;
}

static int chunk(FILE *f, const char *type, const unsigned char *data, size_t n)
{
    unsigned char hdr[8], tail[4];
    uint32_t c;
    be32(hdr, (uint32_t)n);
    memcpy(hdr + 4, type, 4);
    c = crc(0xffffffffu, hdr + 4, 4);
    c = crc(c, data, n) ^ 0xffffffffu;
    be32(tail, c);
    return fwrite(hdr, 1, 8, f) == 8 && (n == 0 || fwrite(data, 1, n, f) == n) && fwrite(tail, 1, 4, f) == 4;
}

int px_png_write(const char *path, const uint32_t *px, int w, int h)
{
    static const unsigned char sig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    size_t row = (size_t)w * 3 + 1, raw_n = row * (size_t)h, blocks = (raw_n + 65534) / 65535, z_n, i, o = 0;
    unsigned char ihdr[13], *raw, *z;
    uint32_t a = 1, b = 0;
    FILE *f;
    int y, x, ok;
    raw = malloc(raw_n);
    z_n = 2 + raw_n + blocks * 5 + 4;
    z = malloc(z_n);
    if (!raw || !z) { free(raw); free(z); return 0; }
    for (y = 0; y < h; y++) {
        unsigned char *r = raw + row * (size_t)y;
        r[0] = 0;
        for (x = 0; x < w; x++) {
            uint32_t p = px[(size_t)y * (size_t)w + (size_t)x];
            r[1 + x * 3] = (unsigned char)(p >> 16); r[2 + x * 3] = (unsigned char)(p >> 8); r[3 + x * 3] = (unsigned char)p;
        }
    }
    z[o++] = 0x78; z[o++] = 0x01;
    for (i = 0; i < raw_n; i += 65535) {
        size_t n = raw_n - i < 65535 ? raw_n - i : 65535;
        z[o++] = i + n == raw_n;
        z[o++] = (unsigned char)n; z[o++] = (unsigned char)(n >> 8);
        z[o++] = (unsigned char)~n; z[o++] = (unsigned char)(~n >> 8);
        memcpy(z + o, raw + i, n);
        o += n;
    }
    for (i = 0; i < raw_n; i++) { a = (a + raw[i]) % 65521; b = (b + a) % 65521; }
    be32(z + o, (b << 16) | a);
    o += 4;
    be32(ihdr, (uint32_t)w); be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8; ihdr[9] = 2; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    f = fopen(path, "wb");
    ok = f && fwrite(sig, 1, 8, f) == 8 && chunk(f, "IHDR", ihdr, 13) && chunk(f, "IDAT", z, o) && chunk(f, "IEND", NULL, 0);
    if (f && fclose(f)) ok = 0;
    free(raw);
    free(z);
    return ok;
}

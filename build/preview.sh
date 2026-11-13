#!/bin/sh
# Renders the app's screens to PNG on the host (no device, no libdrm): both panels,
# the dark and the light theme, the RG DS (640x480) and the RG DS Plus (1024x768).
#   build/preview.sh [out-dir]
# Pixelify Sans: PIXEL_UI_FONTS, else a BananaLinux checkout next to this repo.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-$ROOT/dist/preview}
CC=${HOST_CC:-cc}
if [ -z "${PIXEL_UI_FONTS:-}" ]; then
    for d in "$ROOT/../BananaLinux/ui/assets/rnds/fonts" /usr/share/banana/rnds/fonts; do
        if [ -d "$d" ]; then PIXEL_UI_FONTS=$d; break; fi
    done
fi
[ -n "${PIXEL_UI_FONTS:-}" ] || { echo "set PIXEL_UI_FONTS to the folder with PixelifySans-*.ttf" >&2; exit 1; }
export PIXEL_UI_FONTS
mkdir -p "$OUT" "$ROOT/dist"
UI=$ROOT/app/ui
$CC -O2 -Wall -Wextra -I "$UI/third_party" -o "$ROOT/dist/preview-ui" \
    "$UI/preview.c" "$UI/view.c" "$UI/pixel.c" "$UI/pixel_png.c" "$UI/stb_impl.c" -lm
"$ROOT/dist/preview-ui" "$OUT"

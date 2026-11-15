#!/bin/sh
# Two README library PNGs (grid dark/en, list light/es). build/preview.sh does the full set.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
[ -f "$ROOT/preview.env" ] && set -a && . "$ROOT/preview.env" && set +a
OUT=${1:-$ROOT/dist/preview}
CC=${HOST_CC:-cc}
if [ -z "${PIXEL_UI_FONTS:-}" ]; then
    for d in "$ROOT/../BananaLinux/ui/assets/rnds/fonts" /usr/share/banana/rnds/fonts; do
        [ -d "$d" ] && PIXEL_UI_FONTS=$d && break
    done
fi
[ -n "${PIXEL_UI_FONTS:-}" ] || { echo "set PIXEL_UI_FONTS to the folder with PixelifySans-*.ttf" >&2; exit 1; }
export PIXEL_UI_FONTS
MEDIA="${PREVIEW_MEDIA:-$ROOT/dist/preview-media}"
ROMDIR="${ROM_PREVIEW_SRC:-}"
[ -n "$ROMDIR" ] && [ -d "$ROMDIR" ] || { echo "set ROM_PREVIEW_SRC in preview.env" >&2; exit 1; }
mkdir -p "$MEDIA"
for f in "$ROMDIR"/*.nds; do
    [ -f "$f" ] || continue
    ln -sf "$f" "$MEDIA/$(basename "$f")"
done
export PREVIEW_MEDIA="$MEDIA"
export PREVIEW_LIBRARY_FILE="${PREVIEW_LIBRARY_FILE:-$ROOT/preview-library.list}"
export PREVIEW_STAMP="${PREVIEW_STAMP:-$(TZ=America/Lima date +%Y%m%d-%H%M)}"
export PREVIEW_README_ONLY=1
UI=$ROOT/app/ui
mkdir -p "$OUT"
$CC -O2 -Wall -Wextra -I "$UI/third_party" -o "$ROOT/dist/preview-ui" \
    "$UI/preview.c" "$UI/view.c" "$UI/pixel.c" "$UI/updater.c" "$UI/pixel_png.c" \
    "$UI/pixel_image.c" "$UI/stb_impl.c" -lm
exec "$ROOT/dist/preview-ui" "$OUT"
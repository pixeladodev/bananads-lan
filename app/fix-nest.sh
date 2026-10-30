#!/bin/sh
# Move store/apps/bananads-lan/bananads-lan/ up one level. The UI asks first.
set -eu
INNER=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
OUTER=$(CDPATH= cd -- "$INNER/.." && pwd)
APPS=${BANANADS_MP_APPS:-/data/store/apps}
TILES=${BANANADS_MP_TILES:-/data/store/tiles}
case $INNER in
    "$APPS/bananads-lan/bananads-lan") ;;
    *) echo "not a nested copy"; exit 1 ;;
esac
if [ -f "$OUTER/manifest" ] || [ -d "$OUTER/bin" ]; then
    echo "store/apps/bananads-lan already has the app"
    exit 1
fi
mkdir -p "$TILES"
if [ -f "$OUTER/BananaDS LAN.sh" ]; then
    mv -f "$OUTER/BananaDS LAN.sh" "$TILES/BananaDS LAN.sh"
fi
rm -f "$OUTER/INSTALL.txt" "$OUTER/.DS_Store" 2>/dev/null || true
rm -f "$OUTER"/._* 2>/dev/null || true
cd "$INNER"
for f in .[!.]* ..?* *; do
    [ -e "$f" ] || [ -L "$f" ] || continue
    mv "$f" "$OUTER/$f"
done
cd /
rmdir "$INNER"
APP=$OUTER
. "$APP/common.sh"
mp_clean_junk
echo moved

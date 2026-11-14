#!/bin/sh
# Apps-shelf tile installer (one .sh copied to store/tiles). dev/deploy.sh does not call this.
#   ./build/package.sh ... && ./build/install-onefile.sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ZIP=$(ls -t "$ROOT"/dist/bananads-lan-*.zip 2>/dev/null | head -n 1)
[ -n "$ZIP" ] || { echo "run ./build/package.sh first" >&2; exit 1; }
SUMS="$ROOT/dist/SHA256SUMS"
[ -f "$SUMS" ] || { echo "missing dist/SHA256SUMS (run package.sh)" >&2; exit 1; }
ZIP_SHA=$(awk '{print $1; exit}' "$SUMS")
HDR=$(mktemp)
OUT="$ROOT/dist/Install-BananaDS-LAN.sh"
cat > "$HDR" << 'HDR'
#!/bin/sh
# Install BananaDS LAN on BananaOS: copy this file once into store/tiles, then open it from Apps.
# It unpacks the app into store/apps, adds the real BananaDS LAN tile, and removes this installer.
set -eu
SELF=$0
APPS=${BANANADS_MP_APPS:-/data/store/apps}
TILES=${BANANADS_MP_TILES:-/data/store/tiles}
APP=bananads-lan
TILE='BananaDS LAN.sh'
WANT_SHA=ZIP_SHA_PLACEHOLDER
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
ZIP=$WORK/pkg.zip
tail -n +ARCHIVE_LINE "$SELF" > "$ZIP"
if command -v sha256sum >/dev/null 2>&1; then
    got=$(sha256sum "$ZIP" | awk '{print $1}')
else
    got=$(shasum -a 256 "$ZIP" | awk '{print $1}')
fi
[ "$got" = "$WANT_SHA" ] || exit 1
unzip -oq "$ZIP" -d "$WORK"
DEST="$APPS/$APP"
rm -rf "$DEST"
mkdir -p "$APPS" "$TILES"
cp -a "$WORK/$APP" "$DEST"
cp "$WORK/$TILE" "$TILES/$TILE"
chmod a+x "$DEST/tile.sh" "$DEST/lan.sh" "$DEST/status.sh" "$DEST/run-emu" "$DEST/uninstall.sh" \
    "$DEST/update.py" "$DEST/post-install.sh" "$DEST/pre-remove.sh" \
    "$DEST/ui/bananads-lan" "$DEST/emu/bananads" 2>/dev/null || true
[ -f "$DEST/post-install.sh" ] && sh "$DEST/post-install.sh"
if [ -f "$DEST/tile.png" ]; then
    mkdir -p "$TILES/media/box"
    cp "$DEST/tile.png" "$TILES/media/box/BananaDS LAN.png"
fi
sync 2>/dev/null || true
rm -f "$SELF"
exec sh "$TILES/$TILE"
HDR
MARK=$(($(wc -l < "$HDR") + 1))
sed "s/ARCHIVE_LINE/$MARK/; s/ZIP_SHA_PLACEHOLDER/$ZIP_SHA/" "$HDR" > "$OUT"
cat "$ZIP" >> "$OUT"
chmod 755 "$OUT"
rm -f "$HDR"
echo "$OUT"
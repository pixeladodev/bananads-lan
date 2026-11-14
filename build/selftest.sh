#!/bin/sh
# Script checks for the standalone app. No handheld.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
bad() { echo "FAIL $1"; fail=1; }

for s in common.sh lan.sh status.sh run-emu tile.sh uninstall.sh post-install.sh pre-remove.sh; do
    sh -n "$ROOT/app/$s" || bad "syntax $s"
done
sh -n "$ROOT/tiles/BananaDS LAN.sh" || bad "syntax tile"
grep -q exec "$ROOT/tiles/BananaDS LAN.sh" || bad "tile must exec app tile.sh"

mkdir -p "$T/apps/bananads-lan"
printf '#!/bin/sh\necho OUTER\n' > "$T/apps/bananads-lan/tile.sh"
out=$(BANANADS_MP_APPS=$T/apps sh "$ROOT/tiles/BananaDS LAN.sh")
printf '%s\n' "$out" | grep -qx OUTER || bad "tile did not open the app: $out"

APP=$T/app
mkdir -p "$APP/emu"
cp "$ROOT/app/common.sh" "$ROOT/app/status.sh" "$APP/"
printf 'bin\n' > "$APP/emu/bananads"
printf 'app_version=0.4.0\n' > "$APP/manifest"
printf 'VERSION_ID=0.5.3.4\n' > "$T/os"
printf 'Anbernic RG DS\n' > "$T/model"
BANANADS_MP_OS=$T/os BANANADS_MP_MODEL=$T/model sh "$APP/status.sh" | grep -qx supported=yes \
    || bad "status with emu"
rm -f "$APP/emu/bananads"
BANANADS_MP_OS=$T/os BANANADS_MP_MODEL=$T/model sh "$APP/status.sh" | grep -qx supported=no \
    || bad "status without emu"

APP=$T/apps/bananads-lan BANANADS_MP_TILES=$T/tiles sh -c '
. "'"$ROOT"'/app/common.sh"
printf "x\n" > "$APP/._again"
printf "x\n" > "$APP/.DS_Store"
mp_clean_junk
[ ! -f "$APP/._again" ] && [ ! -f "$APP/.DS_Store" ] || exit 1
' || bad "junk stayed"

sh -n "$ROOT/app/post-install.sh" || bad "syntax post-install.sh"
if command -v python3 >/dev/null 2>&1; then
    python3 "$ROOT/build/update-selftest.py" >/dev/null || bad "updater"
else
    echo "selftest: no python3, the updater is not checked"
fi

[ "$fail" = 0 ] && echo "selftest: passed" || { echo "selftest: failed"; exit 1; }
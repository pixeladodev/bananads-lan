#!/bin/sh
# Checks the launcher block and the version/hash gate. No handheld, no mount.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
bad() { echo "FAIL $1"; fail=1; }

APP=$T/app
mkdir -p "$APP/bin"
printf 'stock-bytes' > "$T/stock"
printf 'our-bytes' > "$APP/bin/bananads"
if command -v sha256sum >/dev/null 2>&1; then
    stock_sha=$(sha256sum "$T/stock" | awk '{print $1}')
    bin_sha=$(sha256sum "$APP/bin/bananads" | awk '{print $1}')
else
    stock_sha=$(shasum -a 256 "$T/stock" | awk '{print $1}')
    bin_sha=$(shasum -a 256 "$APP/bin/bananads" | awk '{print $1}')
fi
cat > "$APP/manifest" << EOF
app_version=0.1.0
bananaos=0.5.0.1
stock_sha_rgds=$stock_sha
stock_sha_rgds_plus=
binary_sha=$bin_sha
EOF
printf 'VERSION_ID=0.5.0.1\n' > "$T/os"
printf 'Anbernic RG DS' > "$T/model"
printf '[psx]\nrun = /usr/libexec/banana/run-retroarch pcsx_rearmed %%ROM%%\n' > "$T/systems.conf"
printf 'BANANADS_LOG_LEVEL=0\n# bananads-lan start\nBANANADS_LINK_SCRIPT=/old/wifi-link\nBANANADS_LAN_EARLY_REPLY=1\n# bananads-lan end\n' > "$T/bananads.env"

APP=$APP \
BANANADS_MP_CONF=$T/systems.conf \
BANANADS_MP_OS=$T/os \
BANANADS_MP_MODEL=$T/model \
BANANADS_MP_STOCK_FILE=$T/stock \
BANANADS_MP_NOMOUNT=1 \
BANANADS_MP_SETTINGS=$T/settings \
BANANADS_MP_ENV=$T/bananads.env \
    sh -c '
set -eu
. "'"$ROOT"'/app/common.sh"
mp_apply
mp_apply
c=$(grep -c "bananads-lan start" "$BANANADS_MP_CONF")
[ "$c" = 1 ] || { echo "marker count $c"; exit 1; }
grep -q "run-nds-lan" "$BANANADS_MP_CONF" || { echo "no launcher"; exit 1; }
grep -q "run-nds %ROM%" "$BANANADS_MP_CONF" || { echo "no stock fallback"; exit 1; }
run=$(sed -n "s/^run = //p" "$BANANADS_MP_CONF" | tail -n 1)
printf "%s\n" "$run" | grep -q awk || { echo "hook lost the mount check"; exit 1; }
printf "%s\n" "$run" | grep -q "umount -l /usr/bin/bananads" || { echo "hook lost the umount"; exit 1; }
printf "%s\n" "$run" | grep -q "//deleted" || { echo "hook misses a deleted mount"; exit 1; }
gone=$(printf "%s\n" "$run" | sed "s/%ROM%/'Game.nds'/g; s|exec /usr/libexec/banana/run-nds|echo STOCK|")
out=$(sh -c "$gone")
printf "%s\n" "$out" | grep -q "^STOCK " || { echo "missing wrapper did not open stock: $out"; exit 1; }
printf "%s\n" "$out" | grep -q WRAPPER && { echo "missing wrapper still ran ours"; exit 1; }
printf "#!/bin/sh\necho WRAPPER \"\$@\"\n" > "$APP/run-nds-lan"
chmod +x "$APP/run-nds-lan"
here=$(printf "%s\n" "$run" | sed "s|/data/store/apps/bananads-lan/run-nds-lan|$APP/run-nds-lan|g; s/%ROM%/'Game.nds'/g; s|exec /usr/libexec/banana/run-nds|echo STOCK|")
out=$(sh -c "$here")
printf "%s\n" "$out" | grep -q "^WRAPPER " || { echo "present wrapper was not used: $out"; exit 1; }
printf "%s\n" "$out" | grep -q "^STOCK " && { echo "present wrapper also ran stock"; exit 1; }
grep -q "\[psx\]" "$BANANADS_MP_CONF" || { echo "psx section lost"; exit 1; }
grep -q "^BANANADS_LOG_LEVEL=0$" "$BANANADS_MP_ENV" || { echo "env key lost"; exit 1; }
grep -q "LINK_SCRIPT" "$BANANADS_MP_ENV" && { echo "link script still required"; exit 1; }
grep -q "EARLY_REPLY" "$BANANADS_MP_ENV" && { echo "early reply still required"; exit 1; }
[ -f "$APP/.mounted" ] || { echo "not mounted"; exit 1; }
mp_prepare_game
[ -f "$APP/.mounted" ] || { echo "prepare dropped an On mount"; exit 1; }
mp_status | grep -qx "enabled=yes"
mp_status | grep -qx "mounted=yes"
mp_status | grep -qx "supported=yes"
mp_revert
grep -q "bananads-lan start" "$BANANADS_MP_CONF" && { echo "marker stayed"; exit 1; }
grep -q "\[psx\]" "$BANANADS_MP_CONF" || { echo "psx lost on revert"; exit 1; }
grep -q "^BANANADS_LOG_LEVEL=0$" "$BANANADS_MP_ENV" || { echo "env key lost on revert"; exit 1; }
[ ! -f "$APP/.mounted" ] || { echo "still mounted"; exit 1; }
mp_prepare_game
[ ! -f "$APP/.mounted" ] || { echo "prepare remounted while off"; exit 1; }
printf "VERSION_ID=0.9.0\n" > "$BANANADS_MP_OS"
mp_apply && { echo "accepted a wrong BananaOS"; exit 1; }
printf "VERSION_ID=0.5.0.1\n" > "$BANANADS_MP_OS"
printf "Anbernic RG DS Plus\n" > "$BANANADS_MP_MODEL"
mp_apply && { echo "accepted a Plus with no stock hash"; exit 1; }
echo OK
' || bad "gate"

# The shelf tile opens with sh, including one folder too deep, and a Mac copy's junk is removed.
mkdir -p "$T/apps/bananads-lan/bananads-lan" "$T/tiles"
printf '#!/bin/sh\necho INNER\n' > "$T/apps/bananads-lan/bananads-lan/tile.sh"
out=$(BANANADS_MP_APPS=$T/apps sh "$ROOT/tiles/BananaDS LAN.sh")
printf '%s\n' "$out" | grep -qx INNER || bad "nested tile did not open: $out"
rm -rf "$T/apps/bananads-lan"
mkdir -p "$T/apps/bananads-lan"
printf '#!/bin/sh\necho OUTER\n' > "$T/apps/bananads-lan/tile.sh"
out=$(BANANADS_MP_APPS=$T/apps sh "$ROOT/tiles/BananaDS LAN.sh")
printf '%s\n' "$out" | grep -qx OUTER || bad "tile did not open the app: $out"
grep -q 'exec sh ' "$ROOT/tiles/BananaDS LAN.sh" || bad "tile execs a script directly"

# Nested copy: move the inner folder up, and the tile that landed beside it.
rm -rf "$T/apps/bananads-lan"
mkdir -p "$T/apps/bananads-lan/bananads-lan/bin" "$T/tiles"
printf 'app_version=0.1.0\n' > "$T/apps/bananads-lan/bananads-lan/manifest"
printf '#!/bin/sh\necho tile\n' > "$T/apps/bananads-lan/BananaDS LAN.sh"
printf 'junk\n' > "$T/apps/bananads-lan/bananads-lan/._note"
printf 'junk\n' > "$T/apps/bananads-lan/bananads-lan/.DS_Store"
cp "$ROOT/app/common.sh" "$T/apps/bananads-lan/bananads-lan/common.sh"
cp "$ROOT/app/fix-nest.sh" "$T/apps/bananads-lan/bananads-lan/fix-nest.sh"
BANANADS_MP_APPS=$T/apps BANANADS_MP_TILES=$T/tiles sh "$T/apps/bananads-lan/bananads-lan/fix-nest.sh" >/dev/null || bad "fix-nest failed"
[ -f "$T/apps/bananads-lan/manifest" ] || bad "fix-nest left the manifest nested"
[ ! -d "$T/apps/bananads-lan/bananads-lan" ] || bad "fix-nest left the inner folder"
[ -f "$T/tiles/BananaDS LAN.sh" ] || bad "fix-nest did not move the tile"
[ ! -f "$T/apps/bananads-lan/._note" ] && [ ! -f "$T/apps/bananads-lan/.DS_Store" ] || bad "fix-nest left Finder junk"

APP=$T/apps/bananads-lan BANANADS_MP_TILES=$T/tiles sh -c '
. "'"$ROOT"'/app/common.sh"
printf "x\n" > "$APP/._again"
printf "x\n" > "$APP/.DS_Store"
mp_clean_junk
[ ! -f "$APP/._again" ] && [ ! -f "$APP/.DS_Store" ] || exit 1
' || bad "junk stayed"

[ "$fail" = 0 ] && echo "selftest: passed" || { echo "selftest: failed"; exit 1; }

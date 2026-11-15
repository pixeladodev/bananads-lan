#!/bin/sh
# Script checks for the emulator runtime package. No handheld.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fail=0
bad() { echo "FAIL $1"; fail=1; }

for s in common.sh lan.sh status.sh run-emu; do
    sh -n "$ROOT/app/$s" || bad "syntax $s"
done

APP=$T/app
mkdir -p "$APP/emu"
cp "$ROOT/app/common.sh" "$ROOT/app/status.sh" "$APP/"
printf 'bin\n' > "$APP/emu/bananads"
printf 'app_version=0.1.0\n' > "$APP/manifest"
printf 'VERSION_ID=0.5.3.4\n' > "$T/os"
printf 'Anbernic RG DS\n' > "$T/model"
BANANADS_MP_OS=$T/os BANANADS_MP_MODEL=$T/model sh "$APP/status.sh" | grep -qx supported=yes \
    || bad "status with emu"
rm -f "$APP/emu/bananads"
BANANADS_MP_OS=$T/os BANANADS_MP_MODEL=$T/model sh "$APP/status.sh" | grep -qx supported=no \
    || bad "status without emu"

[ "$fail" = 0 ] && echo "selftest: passed" || { echo "selftest: failed"; exit 1; }
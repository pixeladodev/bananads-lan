#!/bin/sh
# Pack the pinned emulator runtime for Banana Party (and other consumers).
# Zip root: bananads-lan/ with emu/bananads, common.sh, run-emu, lan.sh, status.sh, manifest.
#   ./build/package.sh [/path/to/bananads]
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BIN=${1:-$ROOT/dist/standalone/bananads}
[ -f "$BIN" ] || { echo "usage: ./build/package.sh [/path/to/bananads]" >&2; exit 2; }
ver=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
want=$(sed -n 's/^binary_sha=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
[ -n "$ver" ] && [ -n "$want" ] || { echo "app/manifest needs app_version and binary_sha" >&2; exit 1; }
if grep -Eq '^[a-z_]+=(TODO|todo)' "$ROOT/app/manifest"; then
    echo "app/manifest still has placeholders:" >&2
    grep -E '^[a-z_]+=(TODO|todo)' "$ROOT/app/manifest" >&2
    [ "${BANANADS_LAN_ALLOW_DRAFT:-}" = 1 ] || exit 1
fi
if command -v sha256sum >/dev/null 2>&1; then
    got=$(sha256sum "$BIN" | awk '{print $1}')
else
    got=$(shasum -a 256 "$BIN" | awk '{print $1}')
fi
[ "$got" = "$want" ] || { echo "binary sha is $got, manifest wants $want" >&2; exit 1; }

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
APP=$STAGE/bananads-lan
mkdir -p "$APP/emu" "$ROOT/dist"

cp "$BIN" "$APP/emu/bananads"
cp "$ROOT/app/common.sh" "$ROOT/app/lan.sh" "$ROOT/app/status.sh" "$ROOT/app/run-emu" \
    "$ROOT/app/manifest" "$APP/"
chmod 755 "$APP/emu/bananads" "$APP/lan.sh" "$APP/status.sh" "$APP/run-emu"

ZIP=$ROOT/dist/bananads-lan-$ver.zip
python3 - "$STAGE" "$ZIP" "$ROOT/dist/SHA256SUMS" << 'PY'
import hashlib, os, sys, zipfile
stage, dest, sums = sys.argv[1], sys.argv[2], sys.argv[3]
with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as z:
    for dirpath, _, files in os.walk(stage):
        for name in files:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, stage).replace(os.sep, "/")
            base = os.path.basename(rel)
            if base.startswith("._") or base == ".DS_Store" or "__MACOSX" in rel.split("/"):
                continue
            info = zipfile.ZipInfo(rel)
            info.external_attr = (os.stat(full).st_mode & 0xFFFF) << 16
            info.compress_type = zipfile.ZIP_DEFLATED
            with open(full, "rb") as fh:
                z.writestr(info, fh.read())
h = hashlib.sha256()
with open(dest, "rb") as fh:
    for chunk in iter(lambda: fh.read(1 << 20), b""):
        h.update(chunk)
with open(sums, "w") as fh:
    fh.write("%s  %s\n" % (h.hexdigest(), os.path.basename(dest)))
with zipfile.ZipFile(dest) as z:
    names = z.namelist()
    tops = set(n.split("/")[0] for n in names)
    if tops != {"bananads-lan"}:
        sys.stderr.write("zip root is %s\n" % ", ".join(sorted(tops)))
        sys.exit(1)
print(dest)
PY
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$ZIP" "$APP/emu/bananads"
else
    shasum -a 256 "$ZIP" "$APP/emu/bananads"
fi
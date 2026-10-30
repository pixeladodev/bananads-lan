#!/bin/sh
# Build the on-screen app and zip it for the BANANA partition.
#   ./build/package.sh /path/to/bananads
# The version in the zip name is app_version from app/manifest.
# The binary must match binary_sha in that file.
# CC: an aarch64 compiler whose sysroot has libdrm. Otherwise the bananads-build:1 image is used.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BIN=${1:-}
[ -n "$BIN" ] && [ -f "$BIN" ] || { echo "usage: ./build/package.sh /path/to/bananads" >&2; exit 2; }
ver=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
want=$(sed -n 's/^binary_sha=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
[ -n "$ver" ] && [ -n "$want" ] || { echo "app/manifest needs app_version and binary_sha" >&2; exit 1; }
if command -v sha256sum >/dev/null 2>&1; then
    got=$(sha256sum "$BIN" | awk '{print $1}')
else
    got=$(shasum -a 256 "$BIN" | awk '{print $1}')
fi
[ "$got" = "$want" ] || { echo "binary sha is $got, manifest wants $want" >&2; exit 1; }

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
APP=$STAGE/store/apps/bananads-lan
mkdir -p "$APP/bin" "$APP/ui" "$STAGE/store/tiles" "$ROOT/dist"

DRM_INC=${DRM_INC:-}
if [ -n "${CC:-}" ]; then
    if [ -z "$DRM_INC" ]; then
        SYS=$("$CC" -print-sysroot 2>/dev/null || true)
        [ -n "$SYS" ] && [ -d "$SYS/usr/include/libdrm" ] && DRM_INC="$SYS/usr/include/libdrm"
    fi
    "$CC" -O2 -Wall -Wextra ${DRM_INC:+-I"$DRM_INC"} -I"$ROOT/app/ui/third_party" -o "$APP/ui/bananads-lan" \
        "$ROOT/app/ui/main.c" "$ROOT/app/ui/stb_impl.c" -ldrm -lm
elif docker image inspect bananads-build:1 >/dev/null 2>&1; then
    docker run --rm --platform linux/amd64 -v "$ROOT:/src:ro" -v "$APP/ui:/out" bananads-build:1 \
        /workspace/bananads-lan-test/aarch64--glibc--stable-2025.08-1/bin/aarch64-buildroot-linux-gnu-gcc \
        -O2 -Wall -Wextra \
        -I/workspace/bananads-lan-test/aarch64--glibc--stable-2025.08-1/aarch64-buildroot-linux-gnu/sysroot/usr/include/libdrm \
        -I/src/app/ui/third_party -o /out/bananads-lan \
        /src/app/ui/main.c /src/app/ui/stb_impl.c -ldrm -lm
else
    echo "set CC to an aarch64 gcc with libdrm, or build where bananads-build:1 exists" >&2
    exit 1
fi

cp "$BIN" "$APP/bin/bananads"
cp "$ROOT/app/common.sh" "$ROOT/app/apply.sh" "$ROOT/app/revert.sh" "$ROOT/app/status.sh" \
    "$ROOT/app/run-nds-lan" "$ROOT/app/tile.sh" "$ROOT/app/uninstall.sh" "$ROOT/app/app.json" \
    "$ROOT/app/manifest" "$APP/"
cp "$ROOT/tiles/BananaDS LAN.sh" "$STAGE/store/tiles/BananaDS LAN.sh"
chmod 755 "$APP/bin/bananads" "$APP/ui/bananads-lan" "$APP/apply.sh" "$APP/revert.sh" \
    "$APP/status.sh" "$APP/run-nds-lan" "$APP/tile.sh" "$APP/uninstall.sh" \
    "$STAGE/store/tiles/BananaDS LAN.sh"

ZIP=$ROOT/dist/bananads-lan-$ver.zip
python3 - "$STAGE" "$ZIP" "$ROOT/dist/SHA256SUMS" << 'PY'
import hashlib, os, sys, zipfile
stage, dest, sums = sys.argv[1], sys.argv[2], sys.argv[3]
with zipfile.ZipFile(dest, "w", zipfile.ZIP_DEFLATED) as z:
    for dirpath, _, files in os.walk(stage):
        for name in files:
            full = os.path.join(dirpath, name)
            rel = os.path.relpath(full, stage)
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
print(dest)
PY
if command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$ZIP" "$APP/bin/bananads" "$APP/ui/bananads-lan"
else
    shasum -a 256 "$ZIP" "$APP/bin/bananads" "$APP/ui/bananads-lan"
fi

#!/bin/bash
# Build the standalone BananaDS LAN emulator: our own BananaDS that lives in the app folder
# (bananads-lan/emu/bananads) and never replaces /usr/bin/bananads.
# Same toolchain and sources as release-build.sh, but self-contained where it is safe:
#   - libstdc++ and libgcc linked in (-static-libstdc++ -static-libgcc), so a BananaOS with
#     another GCC runtime does not break it;
#   - libenet linked in (as in the release build);
#   - glibc, libdrm and libasound stay the console's: glibc and ALSA must match the system
#     (ALSA reads the system's config and bananad's /run/banana/asound.conf), and libdrm.so.2
#     has had a stable ABI for years. The Mali EGL/GLES driver is dlopen'ed from /usr/lib/mali.
# No binary_sha check: the output is dist/standalone/bananads plus its sha256.
#   docker run --rm --platform linux/amd64 -v "$PWD:/src" -w /src debian:trixie-slim bash build/standalone-build.sh
#   BANANADS_SD_SHA=<SuperDrastic commit> to build another commit (default: superdrastic= in app/manifest)
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

manifest() { sed -n "s/^$1=//p" "$ROOT/app/manifest" | head -n 1 | tr -d '\r'; }
SD_SHA=${BANANADS_SD_SHA:-$(manifest superdrastic)}
MELON_SHA=$(manifest melonds)
[ -n "$SD_SHA" ] && [ -n "$MELON_SHA" ]
_NCPU=$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || echo 1)
_MAX_JOBS=$(( _NCPU * 8 / 10 ))
[ "$_MAX_JOBS" -lt 1 ] && _MAX_JOBS=1
JOBS=${BANANADS_BUILD_JOBS:-$_MAX_JOBS}
[ "$JOBS" -gt "$_MAX_JOBS" ] && JOBS=$_MAX_JOBS
[ "$JOBS" -lt 1 ] && JOBS=1
export BANANADS_LTO_JOBS=${BANANADS_LTO_JOBS:-12}
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils tar git \
        cmake ninja-build make pkgconf bash patch python3 \
        dpkg-dev autoconf automake libtool
fi

BOOTLIN_NAME=aarch64--glibc--stable-2025.08-1
BOOTLIN_SHA256=dfb47eee874eef9e8a7fc042eee4e0a183f444b6bcde6a82fef8f009918389c9
TC=${BANANADS_BOOTLIN:-/opt/$BOOTLIN_NAME}
if [ ! -x "$TC/bin/aarch64-linux-gcc" ]; then
    mkdir -p /opt
    curl -fsSLo /tmp/bootlin.tar.xz \
        "https://toolchains.bootlin.com/downloads/releases/toolchains/aarch64/tarballs/$BOOTLIN_NAME.tar.xz"
    echo "$BOOTLIN_SHA256  /tmp/bootlin.tar.xz" | sha256sum -c -
    tar -xJf /tmp/bootlin.tar.xz -C /opt
    rm -f /tmp/bootlin.tar.xz
fi
SYS=$TC/aarch64-buildroot-linux-gnu/sysroot
export PATH="$TC/bin:$PATH"
# .br_real, not the Buildroot wrapper: the wrapper adds -fstack-protector-strong
# and rejects some -L paths. BananaDS's cmake file calls .br_real too.
export CC="$TC/bin/aarch64-linux-gcc.br_real"
export CXX="$TC/bin/aarch64-linux-g++.br_real"
export AR="$TC/bin/aarch64-linux-ar"
export PKG_CONFIG_SYSROOT_DIR=$SYS
export PKG_CONFIG_LIBDIR=$SYS/usr/lib/pkgconfig
export PKG_CONFIG_PATH=

# -flto=auto follows the CPU count, so a 12-core Mac and a GitHub runner
# do not link the same bytes. Pin the partition count. 12 is what produced
# the binary that was checked on the handheld.
pin_flto() {
    local link=$1 real
    real=$(readlink -f "$link")
    if [ -f "${real}.unpinned" ]; then
        return 0
    fi
    mv "$real" "${real}.unpinned"
    cat > "$real" << EOF
#!/bin/bash
jobs=\${BANANADS_LTO_JOBS:-12}
args=()
for a in "\$@"; do
    if [[ "\$a" == "-flto=auto" ]]; then
        args+=("-flto=\${jobs}")
    else
        args+=("\$a")
    fi
done
exec $(printf '%q' "${real}.unpinned") "\${args[@]}"
EOF
    chmod +x "$real"
}
pin_flto "$TC/bin/aarch64-linux-gcc.br_real"
pin_flto "$TC/bin/aarch64-linux-g++.br_real"
export CC="$TC/bin/aarch64-linux-gcc.br_real"
export CXX="$TC/bin/aarch64-linux-g++.br_real"

# libdrm and alsa are the shared libraries already on the console. Debian's
# arm64 packages supply the headers and the link-time .so. libenet is linked
# into the binary: Debian ships no static library, and BananaOS has no
# libenet.so, so it is built from the Debian source package.
DRM_VER=2.4.124-2
ALSA_VER=1.2.14-1+deb13u1
ENET_SRC_VER=1.3.18+ds-1
if [ ! -f "$SYS/usr/lib/libenet.a" ]; then
    dpkg --add-architecture arm64
    cat > /etc/apt/sources.list.d/bananads-src.sources << 'EOF'
Types: deb-src
URIs: http://deb.debian.org/debian
Suites: trixie
Components: main
EOF
    apt-get update
    debs=/tmp/bananads-debs
    rm -rf "$debs"
    mkdir -p "$debs"
    (
        cd "$debs"
        apt-get download \
            "libdrm2:arm64=${DRM_VER}" \
            "libdrm-dev:arm64=${DRM_VER}" \
            "libasound2t64:arm64=${ALSA_VER}" \
            "libasound2-dev:arm64=${ALSA_VER}"
    )
    for deb in "$debs"/*.deb; do
        dpkg-deb -x "$deb" "$SYS"
    done
    for name in libdrm.so libasound.so; do
        ln -sfn "aarch64-linux-gnu/$name" "$SYS/usr/lib/$name"
    done
    src=/tmp/bananads-enet-src
    rm -rf "$src"
    mkdir -p "$src"
    (
        cd "$src"
        apt-get source "enet=${ENET_SRC_VER}"
        cd enet-*
        export CFLAGS="-O2 -g0 -fPIE" CXXFLAGS="-O2 -g0 -fPIE"
        autoreconf -fi
        ./configure --host=aarch64-linux-gnu --prefix=/usr --libdir=/usr/lib \
            --enable-static --disable-shared
        make -j"$JOBS"
        make install DESTDIR="$SYS"
    )
fi
export PKG_CONFIG_LIBDIR=$SYS/usr/lib/pkgconfig:$SYS/usr/lib/aarch64-linux-gnu/pkgconfig

rm -rf /tmp/vmac
mkdir -p /tmp/vmac/src
git -C /tmp/vmac/src init -q
git -C /tmp/vmac/src remote add origin https://github.com/pixeladodev/SuperDrastic.git
git -C /tmp/vmac/src fetch --depth 1 origin "$SD_SHA"
git -C /tmp/vmac/src checkout --detach FETCH_HEAD
mkdir -p /tmp/vmac/src/melonds
curl -fsSLo /tmp/melonds.tar.gz "https://github.com/melonDS-emu/melonDS/archive/$MELON_SHA.tar.gz"
tar -xzf /tmp/melonds.tar.gz -C /tmp/vmac/src/melonds --strip-components=1
bash "$ROOT/build/apply-patches.sh" -s /tmp/vmac/src/melonds /tmp/vmac/src/bananads/patches/melonds '*.patch'

export BANANADS_BOOTLIN=$TC
F="-D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -O2 -g0 -D_FORTIFY_SOURCE=1 -fPIE"
cmake -G Ninja -S /tmp/vmac/src/bananads -B /tmp/vmac/build \
    -DCMAKE_TOOLCHAIN_FILE=/tmp/vmac/src/bananads/cmake/bootlin-a55.cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_SHARED_LIBS=ON \
    -DCMAKE_C_FLAGS="$F" -DCMAKE_CXX_FLAGS="$F" -DCMAKE_ASM_FLAGS="$F" \
    -DCMAKE_EXE_LINKER_FLAGS="-mcpu=cortex-a55 -Wl,-z,relro -fPIE -pie -static-libstdc++ -static-libgcc" \
    -DBANANADS_MELONDS_DIR=/tmp/vmac/src/melonds -DBANANADS_MELONDS_PATCHES=OFF -DBANANADS_JIT=melonds \
    -DBANANADS_SDL=OFF -DBANANADS_ALSA=ON -DBANANADS_KMS=ON -DBANANADS_LAN=ON \
    -DBANANADS_CPU=cortex-a55 -DBANANADS_LTO=ON
ninja -C /tmp/vmac/build -j"$JOBS" bananads
"$TC/bin/aarch64-linux-strip" --remove-section=.comment --remove-section=.note --strip-unneeded \
    -o /tmp/vmac/bananads /tmp/vmac/build/bananads
got=$(sha256sum /tmp/vmac/bananads | awk '{print $1}')
mkdir -p "$ROOT/dist/standalone"
cp /tmp/vmac/bananads "$ROOT/dist/standalone/bananads"
echo "$got  bananads" > "$ROOT/dist/standalone/bananads.sha256"
"$TC/bin/aarch64-linux-readelf" -d /tmp/vmac/bananads | grep -E 'NEEDED|RPATH|RUNPATH' | tee "$ROOT/dist/standalone/needed.txt"
if grep -q 'libstdc++\|libgcc_s' "$ROOT/dist/standalone/needed.txt"; then
    echo "standalone build: libstdc++ or libgcc_s is still a shared dependency" >&2
    exit 1
fi
echo "standalone build: $ROOT/dist/standalone/bananads sha256 $got (SuperDrastic $SD_SHA)"

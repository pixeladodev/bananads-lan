#!/bin/bash
# Build BananaDS and the zip. A tag release runs this.
# The source tree is placed at /tmp/vmac/src: that path is compiled into the binary.
# If the stripped binary's sha256 is not binary_sha in app/manifest, this exits
# and the release is not published.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
cd "$ROOT"

manifest() { sed -n "s/^$1=//p" "$ROOT/app/manifest" | head -n 1 | tr -d '\r'; }
SD_SHA=$(manifest superdrastic)
MELON_SHA=$(manifest melonds)
WANT=$(manifest binary_sha)
[ -n "$SD_SHA" ] && [ -n "$MELON_SHA" ] && [ -n "$WANT" ]

if ! command -v cmake >/dev/null 2>&1 || ! command -v meson >/dev/null 2>&1; then
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends \
        ca-certificates curl xz-utils bzip2 tar git \
        cmake ninja-build make pkgconf meson python3 \
        autoconf automake libtool patch
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
export CC="$TC/bin/aarch64-linux-gcc"
export CXX="$TC/bin/aarch64-linux-g++"
export AR="$TC/bin/aarch64-linux-ar"
export PKG_CONFIG_SYSROOT_DIR=$SYS
export PKG_CONFIG_LIBDIR=$SYS/usr/lib/pkgconfig
export PKG_CONFIG_PATH=

if [ ! -f "$SYS/usr/lib/libenet.a" ]; then
    deps=/tmp/bananads-deps
    rm -rf "$deps"
    mkdir -p "$deps"
    (
        cd "$deps"
        if ! curl -fsSLo enet.tar.gz "http://enet.bespin.org/download/enet-1.3.18.tar.gz"; then
            curl -fsSLo enet.tar.gz "https://github.com/lsalzman/enet/archive/refs/tags/v1.3.18.tar.gz"
        fi
        mkdir enet && tar -xzf enet.tar.gz -C enet --strip-components=1
        cd enet
        [ -f configure ] || autoreconf -fi
        ./configure --host=aarch64-buildroot-linux-gnu --prefix=/usr --enable-static --disable-shared
        make -j"$(nproc)"
        make install DESTDIR="$SYS"
    )
    (
        cd "$deps"
        curl -fsSLo alsa.tar.bz2 "https://www.alsa-project.org/files/pub/lib/alsa-lib-1.2.15.3.tar.bz2"
        mkdir alsa && tar -xjf alsa.tar.bz2 -C alsa --strip-components=1
        cd alsa
        # topology relinks with -L/usr/lib; Bootlin cross-gcc rejects that path
        ./configure --host=aarch64-buildroot-linux-gnu --prefix=/usr \
            --disable-python --enable-static=no --disable-topology
        make -j"$(nproc)"
        make install DESTDIR="$SYS"
    )
    (
        cd "$deps"
        curl -fsSLo libdrm.tar.xz "https://dri.freedesktop.org/libdrm/libdrm-2.4.131.tar.xz"
        mkdir libdrm && tar -xJf libdrm.tar.xz -C libdrm --strip-components=1
        cd libdrm
        cat > cross.txt << EOF
[binaries]
c = '$CC'
cpp = '$CXX'
ar = '$AR'
pkg-config = 'pkg-config'
[host_machine]
system = 'linux'
cpu_family = 'aarch64'
cpu = 'aarch64'
endian = 'little'
[properties]
sys_root = '$SYS'
EOF
        meson setup build --cross-file cross.txt --prefix=/usr \
            -Dintel=disabled -Dradeon=disabled -Damdgpu=disabled -Dnouveau=disabled \
            -Dvmwgfx=disabled -Domap=disabled -Dexynos=disabled -Dfreedreno=disabled \
            -Dtegra=disabled -Dvc4=disabled -Detnaviv=disabled \
            -Dcairo-tests=disabled -Dman-pages=disabled -Dvalgrind=disabled \
            -Dtests=false -Dudev=false
        meson compile -C build
        DESTDIR="$SYS" meson install -C build
    )
fi

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
    -DCMAKE_EXE_LINKER_FLAGS="-mcpu=cortex-a55 -Wl,-z,relro -fPIE -pie" \
    -DBANANADS_MELONDS_DIR=/tmp/vmac/src/melonds -DBANANADS_MELONDS_PATCHES=OFF -DBANANADS_JIT=melonds \
    -DBANANADS_SDL=OFF -DBANANADS_ALSA=ON -DBANANADS_KMS=ON -DBANANADS_LAN=ON \
    -DBANANADS_CPU=cortex-a55 -DBANANADS_LTO=ON
ninja -C /tmp/vmac/build bananads
"$TC/bin/aarch64-linux-strip" --remove-section=.comment --remove-section=.note --strip-unneeded \
    -o /tmp/vmac/bananads /tmp/vmac/build/bananads
got=$(sha256sum /tmp/vmac/bananads | awk '{print $1}')
if [ "$got" != "$WANT" ]; then
    echo "binary sha is $got" >&2
    echo "manifest wants $WANT" >&2
    echo "release stopped: this build is not the binary in app/manifest" >&2
    exit 1
fi

export DRM_INC=$SYS/usr/include/libdrm
"$ROOT/build/package.sh" /tmp/vmac/bananads

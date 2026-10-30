# Source

BananaDS in this app is built from:

- https://github.com/pixeladodev/SuperDrastic/commit/e5847a7522b0fa5722a1bfc93d532ed1e157e7e0
- melonDS https://github.com/melonDS-emu/melonDS/commit/906e9ebb27da8c6a715cd7abab4abfe8a8d29427
- the melonDS patches in that SuperDrastic tree, `bananads/patches/melonds/`

This app does not include `wifi-link`. It plays on the network the console is already on.

The on-screen app's source is `app/ui/`. `stb_truetype.h` is public domain.

`build/release-build.sh` compiles BananaDS with the Bootlin aarch64 glibc stable-2025.08-1 toolchain. libdrm 2.4.124-2 and alsa-lib 1.2.14-1+deb13u1 come from Debian trixie arm64 (`apt-get download`). libenet 1.3.18+ds-1 is built static from that Debian source package, because the package has no static library and BananaOS does not ship libenet. Link-time optimization is pinned to 12 partitions. The script stops if the binary sha256 is not `binary_sha`. `build/apply-patches.sh` is Buildroot's script from 2026.02.3.

This repository is GPLv3 (`LICENSE`) because it is distributed with BananaDS, which is GPLv3.

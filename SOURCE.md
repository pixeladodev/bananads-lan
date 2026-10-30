# Source

BananaDS in this app is built from:

- https://github.com/pixeladodev/SuperDrastic/commit/dc09daac1b91543fd6f4dfc1f9e036ea0f88d634
- melonDS https://github.com/melonDS-emu/melonDS/commit/906e9ebb27da8c6a715cd7abab4abfe8a8d29427
- the melonDS patches in that SuperDrastic tree, `bananads/patches/melonds/`

This app does not include `wifi-link`. It plays on the network the console is already on.

The on-screen app's source is `app/ui/`. `stb_truetype.h` is public domain.

`build/release-build.sh` compiles BananaDS with the Bootlin aarch64 glibc stable-2025.08-1 toolchain, libdrm 2.4.131, alsa-lib 1.2.15.3 and libenet 1.3.18, and stops if the binary sha256 is not `binary_sha`. `build/apply-patches.sh` is Buildroot's script from 2026.02.3.

This repository is GPLv3 (`LICENSE`) because it is distributed with BananaDS, which is GPLv3.

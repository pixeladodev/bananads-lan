# Source

BananaDS in this app is built from:

- https://github.com/pixeladodev/SuperDrastic/commit/b80e24119969ababf9bf8d60e090fc683735e92a (branch `bananads-lan-0.5.3.4`: BananaOS 0.5.3.4's BananaDS, JorreFog/SuperDrastic 5ecd2f7, with the three BananaDS patches BananaOS 0.5.3.4 applies at build time already committed, then Local multiplayer and the saved Nintendo WFC settings on top, then the lobby session). The release build fetches this commit. `BANANADS_SD_OVERLAY` is only for a draft that is not committed yet.
- melonDS https://github.com/melonDS-emu/melonDS/commit/906e9ebb27da8c6a715cd7abab4abfe8a8d29427
- the melonDS patches in that SuperDrastic tree, `bananads/patches/melonds/`

This app does not include `wifi-link`. It plays on the network the console is already on.

The on-screen app's source is `app/ui/`. `stb_truetype.h` is public domain.

`build/release-build.sh` compiles BananaDS with the Bootlin aarch64 glibc stable-2025.08-1 toolchain. libdrm 2.4.124-2 and alsa-lib 1.2.14-1+deb13u1 come from Debian trixie arm64 (`apt-get download`). libenet 1.3.18+ds-1 is built static from that Debian source package, because the package has no static library and BananaOS does not ship libenet. Link-time optimization is pinned to 12 partitions. The script stops if the binary sha256 is not `binary_sha`. `build/apply-patches.sh` is Buildroot's script from 2026.02.3.

This repository is GPLv3 (`LICENSE`) because it is distributed with BananaDS, which is GPLv3.

## Our changes

Reapply this when rebasing onto official BananaDS. It is in the pinned SuperDrastic commit (`src/main.cpp`, `src/frontend_kms.cpp`, `src/menu.c`, `src/menu.h`, `src/lang.c`, `src/alsa.cpp`, `src/lobbyctl.cpp`). It is not a melonDS patch.

The resume save state is slot 0. It is not one of the 8 menu slots. BananaOS writes it on quit and loads it on open, through `resume.on_quit` and `resume.on_start`.

Banana Party is one process (`wifi.lobbyctl_dir`). That process must not save or resume slot 0:

- F+START with a lobby quits without saving. Without a lobby, F+START saves, which is the official default.
- "Quit the game?" still has three options: Keep playing, Save and quit (A by default), and Quit without saving. With a lobby, Save and quit is drawn dim and A does not choose it. Quit without saving stays on X.
- Opening a game with a lobby starts fresh, without loading slot 0. Without a lobby, it resumes, as BananaOS's emulator does.
- SIGUSR1 (the console powers off) still writes slot 0. That is not leaving the room.

A resume saved inside the room would make the next game, or the same game opened again, continue a session that no longer matches the room. The 8 menu slots are unchanged.

The in-game menu's wide row under the six tiles is Local multiplayer (`src/menu.c`). Brightness stays on Quick settings, and Local multiplayer is no longer a Quick settings row. A on that row opens the same Local multiplayer page. Back from it returns to the six tiles.

Auto discovery is Banana Party's setting (`ui.conf` `discover`, and `lobby/discover`, default on). Opening a game while the room is Off and discovery is on asks for Auto for that game only (`src/main.cpp`). `lan_apply_one` does not write Auto into `wifi.local`. Turning discovery off leaves that game Off until someone changes it in BananaDS LAN's in-game menu. The Spanish label Multijugador local is in `src/lang.c` until the language pack ships that line. Host and Join from the menu are still written, as before. When the game ends, an Auto session goes back to Off.

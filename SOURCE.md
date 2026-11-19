# Source

BananaDS in this app is built from:

- https://github.com/pixeladodev/SuperDrastic/commit/40804cf244f100c579b7d342bd01a407d15b50d7 (branch `bananads-lan`: BananaDS from JorreFog/SuperDrastic `e54aaa8` — `5ecd2f7` plus the two upstream commits that BananaOS 0.5.3.5 and later package: `cc145c8` (the in-game menu's clicks follow BananaOS's Menu sounds) and `e54aaa8` (the mic turned up 10x before the gate) — merged with the three BananaDS patches the system applies at build time already committed, then Local multiplayer and the saved Nintendo WFC settings, the Party session, booting the DS firmware for DS Download Play, the DS's MAC from `/etc/machine-id` in the generated and the user's firmware, and Auto Join). The release build fetches this commit. `BANANADS_SD_OVERLAY` is only for a draft that is not committed yet.
- melonDS https://github.com/melonDS-emu/melonDS/commit/906e9ebb27da8c6a715cd7abab4abfe8a8d29427
- the melonDS patches in that SuperDrastic tree, `bananads/patches/melonds/`

This app does not include `wifi-link`. It plays on the network the console is already on.

The on-screen app's source is `app/ui/`. `stb_truetype.h` is public domain.

`build/release-build.sh` compiles BananaDS with the Bootlin aarch64 glibc stable-2025.08-1 toolchain. libdrm 2.4.124-2 and alsa-lib 1.2.14-1+deb13u1 come from Debian trixie arm64 (`apt-get download`). libenet 1.3.18+ds-1 is built static from that Debian source package, because the package has no static library and BananaOS does not ship libenet. libstdc++ and libgcc are linked in (`-static-libstdc++ -static-libgcc`), so the player does not depend on the system's GCC runtime; glibc, libdrm and libasound stay the console's. Link-time optimization is pinned to 12 partitions. The script stops if the binary sha256 is not `binary_sha`. `build/apply-patches.sh` is Buildroot's script from 2026.02.3.

This repository is GPLv3 (`LICENSE`) because it is distributed with BananaDS, which is GPLv3.

## Our changes

Reapply this when merging or rebasing onto official BananaDS. It is in the pinned SuperDrastic commit (`src/main.cpp`, `src/frontend_kms.cpp`, `src/menu.c`, `src/menu.h`, `src/lang.c`, `src/alsa.cpp`, `src/lobbyctl.cpp`). It is not a melonDS patch.

The upstream pin was merged, not rebased, on 19-nov-2026 (`40804cf`, parents `87f6161` and `e54aaa8`): the merge was clean and every item below was still there afterwards. The three BananaDS patches the system applies at build time (the help line above the Back button, the remappable pad buttons and the left stick's 45-degree octants) are committed in our tree, so they are not applied again on top of it.

The resume save state is slot 0. It is not one of the 8 menu slots. BananaOS writes it on quit and loads it on open, through `resume.on_quit` and `resume.on_start`.

Banana Party is one process (`wifi.lobbyctl_dir`). That process must not save or resume slot 0:

- F+START in the Party process quits without saving. Outside it, F+START saves, which is the official default.
- "Quit the game?" still has three options: Keep playing, Save and quit (A by default), and Quit without saving. In the Party process, Save and quit is drawn dim and A does not choose it. Quit without saving stays on X.
- Opening a game in the Party process starts fresh, without loading slot 0. Outside it, it resumes, as BananaOS's emulator does.
- SIGUSR1 (the console powers off) still writes slot 0. That is not leaving the Party.

A resume saved inside the Party would make the next game, or the same game opened again, continue a session that no longer matches the Party. The 8 menu slots are unchanged.

The in-game menu's wide row under the six tiles is Local multiplayer (`src/menu.c`). Brightness stays on Quick settings, and Local multiplayer is no longer a Quick settings row. A on that row opens the same Local multiplayer page. Back from it returns to the six tiles.

Auto discovery is Banana Party's setting (`ui.conf` `discover`, and `lobby/discover`, default on). Opening a game while the Party is Off and discovery is on asks for Auto for that game only (`src/main.cpp`). Opening the DS menu (`@firmware`, DS Download Play) the same way asks for Auto Join instead. `lan_apply_one` does not write Auto into `wifi.local`. Turning discovery off leaves that game Off until someone changes it in BananaDS LAN's in-game menu. The Spanish lines for Multijugador local, Auto Join and the Party messages are in `src/lang.c` until the language pack ships them. Host and Join from the menu are still written, as before. When the game ends, an Auto session goes back to Off.

Auto Join (Spanish: Unirse automáticamente) is a Local multiplayer mode: `wifi.local = autojoin`, or Auto Join on the in-game menu's Mode row. It is Auto that never hosts. It looks for a host and joins it; where Auto would host, it keeps looking. Use it on a console that should only receive, like the DS menu's Download Play. The menu does not write it into `wifi.local`, like Auto.

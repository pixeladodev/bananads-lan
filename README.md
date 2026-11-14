# BananaDS LAN

Local multiplayer for Nintendo DS games on BananaOS. Two or more consoles on the same Wi-Fi network can race, battle, trade and chat as if they were linked by DS wireless, using BananaDS.

Install it once, turn it on from the Apps shelf, and play.

## Requirements

- Anbernic RG DS or RG DS Plus
- BananaOS 0.5.3.4 (for BananaOS 0.5.2.2, use BananaDS LAN 0.2.0; for 0.5.0.1, 0.1.0)
- All consoles connected to the same Wi-Fi network
- BananaDS as the Nintendo DS emulator: Settings > Nintendo DS > Emulator > BananaDS

On any other BananaOS version the app does nothing and your games keep the BananaDS that came with the system.

## Install

Download `bananads-lan-<version>.zip` from [Releases](../../releases) and extract it. You get:

- the folder `bananads-lan`
- the file `BananaDS LAN.sh`
- `INSTALL.txt` (these same steps, in English and Spanish)

**Recommended:** copy `Install-BananaDS-LAN.sh` from the release into `store/tiles`, reboot once, open Apps, and run the installer tile.

Or extract the zip and copy the folder `bananads-lan` into `store/apps` and `BananaDS LAN.sh` into `store/tiles`. Copy only those items; do not replace the whole `store` folder. Reboot once and open Apps.

Full steps (file sharing, SMB, microSD) are in `INSTALL.txt` inside the zip.

## Use

Open BananaDS LAN from Apps:

- **A** turns Local multiplayer on or off, or, on **Updates**, looks for and installs a newer version
- **SELECT** shows About: version, author, source code and credits
- **Y** uninstalls the app (it asks first)
- **B** goes back to the menu

When it is on, every DS game you open uses BananaDS with Local multiplayer. It stays on after a reboot until you turn it off or uninstall it.

Local multiplayer is set to **Auto**, so it is plug and play: open the same game on each console, go to the game's own wireless or multiplayer mode, and the consoles find each other. You can also pick Host or Join yourself from the in-game menu, on the Local multiplayer page.

## Updates

The app looks for a newer release on GitHub each time it opens; the **Updates** row says what it found. A on it, then A again, downloads the new zip, checks it against the SHA-256 GitHub lists for it, and installs it the way the Store does (a new folder beside the old one, then a swap), so a failed download or a damaged zip changes nothing. If Local multiplayer was on, it stays on with the new BananaDS. Then A opens the new version. A release made for another BananaOS is not installed over one that works.

`app/update.py` does the work (`python3 update.py check` or `install`); `build/update-selftest.py` runs it against a fake GitHub on localhost.

## Uninstall

Open BananaDS LAN and press **Y**. It turns itself off and removes its folder and tile. Your games go back to the BananaDS that came with BananaOS.

If you delete the folder by hand instead, DS games still open with the original BananaDS.

## Good to know

- Play close to the router. A busy or distant network can cause slowdowns.
- Turning the app off or uninstalling it changes nothing else on the system.

## Built with AI agents

BananaDS LAN is an open source project built with the help of AI coding agents, following good engineering practices: every change is reviewed, and each release is tested on real RG DS and RG DS Plus consoles before it is published.

Even so, as with any software, it may have bugs or behave unexpectedly on your setup. It is provided "as is", without warranty of any kind (see `LICENSE`). Use it at your own risk, and keep a backup of your saves and your microSD card.

## Source and license

BananaDS LAN is free software under the GNU GPL v3 (see `LICENSE` and `NOTICE`). It is built from BananaDS (SuperDrastic) and melonDS. `SOURCE.md` lists the exact source code and how to build it.

### Building

`./build/package.sh /path/to/bananads` builds the app and writes `dist/bananads-lan-<version>.zip` and `dist/SHA256SUMS`. Releases are built by GitHub Actions when a `v*` tag is pushed. `./build/selftest.sh` runs checks without a console.

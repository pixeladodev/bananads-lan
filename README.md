# BananaDS LAN

Local multiplayer for Nintendo DS games on BananaOS. Two or more consoles on the same Wi-Fi network can race, battle, trade and chat as if they were linked by DS wireless, using BananaDS.

Install it once, turn it on from the Apps shelf, and play.

## Requirements

- Anbernic RG DS or RG DS Plus
- BananaOS 0.5.0.1
- All consoles connected to the same Wi-Fi network
- BananaDS as the Nintendo DS emulator: Settings > Nintendo DS > Emulator > BananaDS

On any other BananaOS version the app does nothing and your games keep the BananaDS that came with the system.

## Install

Download `bananads-lan-<version>.zip` from [Releases](../../releases) and extract it. You get:

- the folder `bananads-lan`
- the file `BananaDS LAN.sh`
- `INSTALL.txt` (these same steps, in English and Spanish)

Copy them to the console:

1. On the console, turn on File sharing in Settings > Network. The address is shown on that screen. Log in as Guest, or with the name and password under Sharing name and password (by default `banana` and `linux`).
2. Open the shared folder named `banana` from your computer or phone:
   - Windows: `\\<address>\banana` in File Explorer
   - macOS: Finder > Go > Connect to Server > `smb://<address>/banana`
   - Android (CX File Explorer): Network > New location > SMB > `banana`
   - Or put the microSD card in a reader and open the partition named BANANA
3. Copy the folder `bananads-lan` into `store/apps`, and the file `BananaDS LAN.sh` into `store/tiles`:

   ```
   store/
     apps/
       bananads-lan/
     tiles/
       BananaDS LAN.sh
   ```

   Copy only those two items. Do not drag or replace the whole `store` folder, or you will lose your other apps.
4. Reboot the console once. BananaDS LAN now appears in Apps.

If you see `store/apps/bananads-lan/bananads-lan/`, the folder went one level too deep. The app still opens and offers to fix it.

## Use

Open BananaDS LAN from Apps:

- **A** turns Local multiplayer on or off
- **Y** uninstalls the app (it asks first)
- **B** goes back to the menu

When it is on, every DS game you open uses BananaDS with Local multiplayer. It stays on after a reboot until you turn it off or uninstall it.

Local multiplayer is set to **Auto**, so it is plug and play: open the same game on each console, go to the game's own wireless or multiplayer mode, and the consoles find each other. You can also pick Host or Join yourself from the in-game menu, on the Local multiplayer page.

## Uninstall

Open BananaDS LAN and press **Y**. It turns itself off and removes its folder and tile. Your games go back to the BananaDS that came with BananaOS.

If you delete the folder by hand instead, DS games still open with the original BananaDS.

## Good to know

- Play close to the router. A busy or distant network can cause slowdowns.
- Turning the app off or uninstalling it changes nothing else on the system.

## Source and license

BananaDS LAN is free software under the GNU GPL v3 (see `LICENSE` and `NOTICE`). It is built from BananaDS (SuperDrastic) and melonDS. `SOURCE.md` lists the exact source code and how to build it.

### Building

`./build/package.sh /path/to/bananads` builds the app and writes `dist/bananads-lan-<version>.zip` and `dist/SHA256SUMS`. Releases are built by GitHub Actions when a `v*` tag is pushed. `./build/selftest.sh` runs checks without a console.

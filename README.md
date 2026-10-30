# BananaDS LAN

Local multiplayer for BananaDS on BananaOS, over the Wi-Fi network the console is already on. Turn it on or off from an app on the Apps shelf.

Works with BananaOS 0.5.0.1 on the Anbernic RG DS and the RG DS Plus. Other versions are left on the BananaDS that came with the OS.

There is no direct link in this app, and no Connection row. Two consoles on the same network use Host and Join. `BANANADS_LAN_EARLY_REPLY=0` turns the early reply off. Anything else leaves it on.

## Install

Unzip onto the BANANA partition (the one that already has `roms` and `banana`), so these paths exist:

- `store/apps/bananads-lan/`
- `store/tiles/BananaDS LAN.sh`

Open the Apps shelf. If the tile is not there, reboot once.

In Settings > Nintendo DS > Emulator, choose BananaDS. The app does not change that setting.

A toggles On and Off. Y asks, then uninstalls: Off, then the app and its tile are deleted. B goes back to the menu.

On, the next DS game is this BananaDS, including when the menu was already open. Off, the next game is the copy that came with the OS, including when the menu was already open.

Deleting the app folder while it is on does not stop DS games from opening. The launcher checks that this app is still there. If it is gone, it drops this app's mount when that mount is the one in place, and opens the BananaDS that came with the OS.

## What On and Off change

On: the DS launcher in `banana/systems.conf` becomes this app's wrapper, and the app's BananaDS is mounted on `/usr/bin/bananads`. Off: that launcher line is removed and the mount is dropped. A BananaOS version or a stock BananaDS this app does not list is not mounted.

## Tried on the handhelds

On BananaOS 0.5.0.1, on an RG DS and an RG DS Plus: the tile showed up, On mounted this BananaDS, two consoles hosted and joined a game on the same network, Off went back to the BananaDS that came with the OS, and Uninstall left the next game on that copy.

Deleting the app folder while it is on leaves the launcher line in place. The next game drops this app's mount, including when the mounted file is already gone, and opens the BananaDS that came with the OS.

The binary matches a build of `pixeladodev/SuperDrastic` `dc09daa` (sha256 below).

Both handhelds' stock BananaDS sha256 values are in `app/manifest`.

```
88a435f4a38b4f9253a233415e64052b5a771c96eeb9c66cb0aa8cbe26edde94  bananads
```

## Build the zip

`./build/package.sh /path/to/bananads` compiles the on-screen app and writes `dist/bananads-lan-<version>.zip` plus `dist/SHA256SUMS`. The version comes from `app/manifest`. The binary's sha256 must match `binary_sha` in that file. It uses `CC` (aarch64, with libdrm) or the `bananads-build:1` image when that is present.

`./build/selftest.sh` checks the launcher block and the version gate without a handheld.

`./dev/deploy.sh` copies the built tree onto consoles. It does not commit. Set `CONSOLES` or `dev/consoles.local`.

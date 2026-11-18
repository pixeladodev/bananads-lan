# BananaDS LAN

BananaDS with local wireless multiplayer for the Anbernic RG DS and RG DS Plus on BananaOS.

It is not installed on its own: it is the emulator used by [Banana Party](https://github.com/pixeladodev/banana-party), where you browse your games and play together.

## Local multiplayer

In the in-game menu, Local multiplayer's Mode is Off, Auto, Auto Join, Host or Join:

- **Auto:** joins a host of the same game, or hosts one if nobody is there yet.
- **Auto Join** (Spanish: *Unirse automáticamente*): looks for a host and joins it. It never hosts; it keeps looking until it finds one. Banana Party's DS Download Play uses it when auto discovery is on.
- **Host** and **Join:** host a session, or look for hosts and pick one.

The same modes go in `bananads.conf` as `wifi.local = off | auto | autojoin | host | join`.

Licensed under GPL v3. See `LICENSE`.

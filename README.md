# BananaDS LAN

Standalone **BananaDS** build for BananaOS handhelds with **local wireless multiplayer** on the same network. You do not install this repository on the shelf. It ships inside **[Banana Lobby](https://github.com/pixeladodev/banana-lobby)** at package time; play and browse your library there.

**Devices:** Anbernic **RG DS** and **RG DS Plus**. Built and checked against **BananaOS 0.5.3.4** (also tested on **BananaOS 0.5.2.2** where the manifest pins allow).

## What is in a release zip

- `bananads-lan/emu/bananads` — the emulator binary (pinned `binary_sha` in `app/manifest`)
- `run-emu`, `lan.sh`, `status.sh`, `common.sh` — runtime helpers the host app copies beside the binary

## Build locally

From a Linux VM or container with network (same flow as CI):

```bash
docker run --rm --platform linux/amd64 -v "$PWD:/src" -w /src debian:trixie-slim bash build/release-build.sh
```

Output: `dist/bananads-lan-<version>.zip` and `dist/SHA256SUMS`. A tagged release must match `binary_sha` in `app/manifest` or the build stops.

Draft compile (report sha only, no zip):

```bash
docker run --rm --platform linux/amd64 -e BANANADS_LAN_ALLOW_DRAFT=1 -v "$PWD:/src" -w /src debian:trixie-slim bash build/release-build.sh
```

`build/standalone-build.sh` builds the same sources without enforcing the manifest pin (for iteration).

`build/apply-patches.sh` applies the tree’s patch set when fetching sources inside `release-build.sh`.

## Layout

| Path | Role |
|------|------|
| `app/manifest` | Version, pinned binary hash, source commits |
| `app/run-emu` | How a host app launches a game on this binary |
| `build/release-build.sh` | Fetch sources, compile, pack zip |
| `build/package.sh` | Zip the binary and scripts after a successful compile |

## License

GPL v3 (`LICENSE`). Built from the pinned commits recorded in `app/manifest`; see release notes for checksums.
#!/bin/bash
# Print release body: release-notes/<ver>.md plus checksum and GPL links from the manifest.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
"$ROOT/build/check-release-notes.sh"
ver=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
sd=$(sed -n 's/^superdrastic=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
melon=$(sed -n 's/^melonds=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
zip="$ROOT/dist/bananads-lan-${ver}.zip"
sums="$ROOT/dist/SHA256SUMS"
[ -f "$zip" ] || { echo "assemble-release-notes: missing $zip (run release-build first)" >&2; exit 1; }
[ -f "$sums" ] || { echo "assemble-release-notes: missing $sums" >&2; exit 1; }
zip_sha=$(awk '{print $1; exit}' "$sums")
cat "$ROOT/release-notes/${ver}.md"
echo
echo '---'
echo
echo '## Checksums and source (GPLv3)'
echo
printf 'SHA256 of `bananads-lan-%s.zip`: `%s`\n\n' "$ver" "$zip_sha"
echo 'Source:'
printf '%s\n' "- https://github.com/pixeladodev/SuperDrastic/commit/${sd}"
printf '%s\n' "- https://github.com/melonDS-emu/melonDS/commit/${melon}"
echo '- melonDS patches in that SuperDrastic tree: `bananads/patches/melonds/`'
echo '- `build/release-build.sh`, `build/package.sh`, and `build/apply-patches.sh` in this repository'
echo '- `LICENSE`'

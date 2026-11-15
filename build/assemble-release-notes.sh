#!/bin/bash
# Print release body: release-notes/<ver>.md plus checksum footer.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
"$ROOT/build/check-release-notes.sh" >&2
ver=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
zip="$ROOT/dist/bananads-lan-${ver}.zip"
sums="$ROOT/dist/SHA256SUMS"
[ -f "$zip" ] || { echo "assemble-release-notes: missing $zip (run release-build first)" >&2; exit 1; }
[ -f "$sums" ] || { echo "assemble-release-notes: missing $sums" >&2; exit 1; }
zip_sha=$(awk '{print $1; exit}' "$sums")
cat "$ROOT/release-notes/${ver}.md"
echo
echo
echo '---'
echo
echo '## Checksums'
echo
printf 'SHA256 of `bananads-lan-%s.zip`: `%s`\n\n' "$ver" "$zip_sha"
echo "Source: this repository (\`LICENSE\`). Binary commit pins are in \`app/manifest\`."
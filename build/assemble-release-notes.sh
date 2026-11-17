#!/bin/bash
# Print the release body: release-notes/<ver>.md as written.
# Only the notes go to stdout; the check's messages go to stderr.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
"$ROOT/build/check-release-notes.sh" >&2
ver=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
zip="$ROOT/dist/bananads-lan-${ver}.zip"
[ -f "$zip" ] || { echo "assemble-release-notes: missing $zip (run release-build first)" >&2; exit 1; }
cat "$ROOT/release-notes/${ver}.md"

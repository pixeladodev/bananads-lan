#!/bin/bash
# Fails if release-notes/<app_version>.md is missing or not in the release shape:
# English, exactly two sections ("What changed", then "Installation"), and no BananaOS version
# named as a requirement.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ver=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
[ -n "$ver" ] || { echo "check-release-notes: app/manifest has no app_version" >&2; exit 1; }
file="$ROOT/release-notes/${ver}.md"
[ -f "$file" ] || {
    echo "check-release-notes: missing $file" >&2
    echo "Every release needs notes in release-notes/<version>.md (English)." >&2
    exit 1
}
fail() { echo "check-release-notes: $file $*" >&2; exit 1; }
chars=$(wc -c < "$file" | tr -d ' ')
lines=$(grep -c '[^[:space:]]' "$file" || true)
if [ "$chars" -lt 300 ] || [ "$lines" -lt 5 ]; then
    fail "is too short ($chars bytes, $lines non-empty lines)"
fi
heads=$(grep -E '^#+[[:space:]]' "$file" | sed -E 's/^#+[[:space:]]+//; s/[[:space:]]+$//' | tr '\n' '|')
[ "$heads" = "What changed|Installation|" ] || fail "must have exactly two headings, What changed then Installation (has: ${heads:-none})"
grep -q 'DRAFT' "$file" && fail "is marked DRAFT; finish it and remove the marker before a release"
grep -q 'check-release-notes' "$file" && fail "carries a check-release-notes line"
grep -q 'RG DS' "$file" || fail "must mention RG DS"
if grep -Eq 'BananaOS\**[[:space:]]+\**v?[0-9]+\.[0-9]' "$file"; then
    fail "names a BananaOS version; describe what is needed, not a system version"
fi
for bad in "BANANADS_LINK" "wifi-link" "Connection row" "Nearby consoles" "room.env" "88a435f" "620096ab" "5ddfa244"; do
    grep -q "$bad" "$file" && fail "must not mention $bad (internal or out-of-scope detail)"
done
echo "check-release-notes: ok ($file)" >&2

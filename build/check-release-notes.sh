#!/bin/bash
# Fails if release-notes/<app_version>.md is missing or too thin for a public release.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ver=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
[ -n "$ver" ] || { echo "check-release-notes: app/manifest has no app_version" >&2; exit 1; }
file="$ROOT/release-notes/${ver}.md"
[ -f "$file" ] || {
    echo "check-release-notes: missing $file" >&2
    echo "Every release needs quality notes in release-notes/<version>.md (English)." >&2
    exit 1
}
chars=$(wc -c < "$file" | tr -d ' ')
lines=$(grep -c '[^[:space:]]' "$file" || true)
if [ "$chars" -lt 1200 ] || [ "$lines" -lt 28 ]; then
    echo "check-release-notes: $file is too short ($chars bytes, $lines non-empty lines)" >&2
    echo "Release notes must be substantive, not a placeholder." >&2
    exit 1
fi
os=$(sed -n 's/^bananaos=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
[ -n "$os" ] || { echo "check-release-notes: app/manifest has no bananaos" >&2; exit 1; }
if grep -q 'DRAFT' "$file"; then
    echo "check-release-notes: $file is marked DRAFT; finish it and remove the marker before a release" >&2
    exit 1
fi
for need in "BananaOS" "$os" "RG DS"; do
    grep -q "$need" "$file" || {
        echo "check-release-notes: $file must mention $need" >&2
        exit 1
    }
done
for bad in "BANANADS_LINK" "wifi-link" "Connection row" "Nearby consoles" "88a435f" "620096ab" "5ddfa244"; do
    if grep -q "$bad" "$file"; then
        echo "check-release-notes: $file must not mention $bad (internal or out-of-scope detail)" >&2
        exit 1
    fi
done
echo "check-release-notes: ok ($file)"

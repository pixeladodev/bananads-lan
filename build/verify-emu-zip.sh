#!/bin/bash
# Fail if the packed zip still looks like the old shelf app.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ver=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
zip="$ROOT/dist/bananads-lan-${ver}.zip"
[ -f "$zip" ] || { echo "verify-emu-zip: missing $zip" >&2; exit 1; }
for bad in 'ui/' 'Install-' 'BananaDS LAN.sh' 'update.py' 'uninstall.sh' 'tile.sh'; do
    if unzip -l "$zip" | grep -q "$bad"; then
        echo "verify-emu-zip: $zip must not contain $bad" >&2
        exit 1
    fi
done
for need in 'bananads-lan/emu/bananads' 'bananads-lan/run-emu' 'bananads-lan/lan.sh' 'bananads-lan/status.sh'; do
    unzip -l "$zip" | grep -q "$need" || { echo "verify-emu-zip: missing $need" >&2; exit 1; }
done
echo "verify-emu-zip: ok ($zip is emulator-only)"
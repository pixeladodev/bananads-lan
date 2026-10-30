#!/bin/bash
# Copy this app onto the consoles, over the installed path. Does not commit.
#   CONSOLES="ip ip" ./dev/deploy.sh -b /path/to/bananads
#   ./dev/deploy.sh -b /path/to/bananads     # consoles from dev/consoles.local
# Without -b, app/bin/bananads is used when it is already there.
# If the app is On, apply.sh runs again so the new binary is mounted.
set -euo pipefail
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BIN=
while getopts b: o; do case $o in b) BIN=$OPTARG ;; *) sed -n '2,8s/^# \{0,1\}//p' "$0"; exit 2 ;; esac; done
if [ -z "$BIN" ]; then
    BIN=$ROOT/app/bin/bananads
    [ -f "$BIN" ] || { echo "deploy: pass -b /path/to/bananads" >&2; exit 2; }
fi
if [ -z "${CONSOLES:-}" ] && [ -f "$ROOT/dev/consoles.local" ]; then
    CONSOLES=$(grep -v '^[[:space:]]*#' "$ROOT/dev/consoles.local" | tr '\n' ' ')
fi
[ -n "${CONSOLES:-}" ] || { echo "deploy: set CONSOLES or dev/consoles.local" >&2; exit 2; }
KEY=${BANANADS_LAN_KEY:-$HOME/.ssh/bananaos_rgds}

"$ROOT/build/package.sh" "$BIN" >/dev/null
ver=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
ZIP=$ROOT/dist/bananads-lan-$ver.zip
[ -f "$ZIP" ] || { echo "deploy: no $ZIP" >&2; exit 1; }
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
unzip -q -d "$STAGE" "$ZIP"

one() {
    local ip=$1
    COPYFILE_DISABLE=1 tar -C "$STAGE" -cf - store | ssh -i "$KEY" -o IdentitiesOnly=yes -o ConnectTimeout=8 -o BatchMode=yes "root@$ip" 'tar -C /data -xf -'
    ssh -i "$KEY" -o IdentitiesOnly=yes -o ConnectTimeout=8 -o BatchMode=yes "root@$ip" '
        set -eu
        A=/data/store/apps/bananads-lan
        st=$($A/status.sh)
        echo "$st" | grep -qx "enabled=yes" && $A/apply.sh || true
        sha256sum $A/bin/bananads
        $A/status.sh
    '
}
pids=
for ip in $CONSOLES; do
    one "$ip" > "$STAGE/out-$ip.txt" 2>&1 &
    pids="$pids $!"
done
st=0
for p in $pids; do wait "$p" || st=1; done
for ip in $CONSOLES; do
    echo "==== $ip ===="
    cat "$STAGE/out-$ip.txt"
done
exit $st

#!/bin/sh
# Off, then delete this app and its Apps tile. The UI asks first.
set -eu
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
case $APP in
    /data/store/apps/bananads-lan) ;;
    *) echo "refusing to remove $APP" >&2; exit 1 ;;
esac
. "$APP/common.sh"
mp_revert
rm -f "/data/store/tiles/BananaDS LAN.sh" "/data/store/tiles/._BananaDS LAN.sh" \
    "/data/store/tiles/.DS_Store"
cd /
rm -rf "$APP"
rm -f "/data/store/apps/._bananads-lan" "/data/store/apps/.DS_Store"

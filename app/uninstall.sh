#!/bin/sh
# Delete this app and its Apps tile (and anything an older version left behind). The UI asks first.
set -eu
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
case $APP in
    /data/store/apps/bananads-lan) ;;
    *) echo "refusing to remove $APP" >&2; exit 1 ;;
esac
. "$APP/common.sh"
mp_legacy_cleanup || true
rm -f "/data/store/tiles/BananaDS LAN.sh" "/data/store/tiles/._BananaDS LAN.sh" \
    "/data/store/tiles/.DS_Store" "/data/store/tiles/media/box/BananaDS LAN.png"
rmdir /data/store/tiles/media/box /data/store/tiles/media 2>/dev/null || true
cd /
rm -rf "$APP"
rm -f "/data/store/apps/._bananads-lan" "/data/store/apps/.DS_Store"
sync 2>/dev/null || true

#!/bin/sh
# Local multiplayer on or off for the games the app opens (its own BananaDS's wifi.local: on is Auto, as BananaDS
# starts with no saved value; a game's menu can still change it while it runs).
#   lan.sh on|off
set -eu
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$APP/common.sh"
case ${1:-} in
    on) mp_enabled && exit 0; mp_lan_set auto ;;
    off) mp_lan_set off ;;
    *) echo "usage: lan.sh on|off" >&2; exit 2 ;;
esac

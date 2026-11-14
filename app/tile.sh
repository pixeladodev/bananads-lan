#!/bin/sh
# Apps shelf entry. The menu has already let go of both panels.
# sh, not a direct exec: a copy over SMB can drop the executable bit on scripts.
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$APP/common.sh"
mp_clean_junk
chmod a+x "$APP/ui/bananads-lan" "$APP/apply.sh" "$APP/revert.sh" "$APP/status.sh" \
    "$APP/run-nds-lan" "$APP/run-emu" "$APP/emu/bananads" "$APP/uninstall.sh" "$APP/fix-nest.sh" "$APP/update.py" "$APP/post-install.sh" 2>/dev/null || true
# the shelf's picture for this tile: banana-ui looks for store/tiles/media/box/<tile name>.png
BOX=/data/store/tiles/media/box
if [ -f "$APP/tile.png" ] && ! cmp -s "$APP/tile.png" "$BOX/BananaDS LAN.png" 2>/dev/null; then
    mkdir -p "$BOX" 2>/dev/null && cp "$APP/tile.png" "$BOX/BananaDS LAN.png" 2>/dev/null || true
fi
# A game picked in the app's list: the UI writes its path to $PLAY and exits with 76; the game runs on the app's own
# BananaDS (run-emu) and the list opens again when it ends. For testing over SSH, a path written to
# /run/banana/bananads-lan-launch is played at once (once), as banana-ui's /run/banana/launch does for the menu.
PLAY=/tmp/bananads-lan.play
rm -f /tmp/bananads-lan-game.pid    # (a copy left by a session that ended without putting it back)
TEST=/run/banana/bananads-lan-launch
# the app's own entry in game.pid, put back after a game (run-emu points it at the game while it runs)
restore_pid() {
    [ -f /tmp/bananads-lan-game.pid ] && mv -f /tmp/bananads-lan-game.pid /run/banana/game.pid 2>/dev/null
    return 0
}
play() {
    rm -f "$PLAY"
    sh "$APP/run-emu" "$1"
    rc=$?
    restore_pid
    # the app's clocks again (systems.conf's perf for [storeapps], as banana-ui set them when the app started)
    command -v banana-perf >/dev/null 2>&1 && banana-perf balanced 2>/dev/null
    [ "$rc" = 0 ] || echo "run-emu: exit $rc" >&2
}
if [ -f "$TEST" ]; then
    rom=$(head -n 1 "$TEST"); rm -f "$TEST"
    play "$rom"
    exit 0
fi
ui_arg=
while :; do
    rm -f "$PLAY"
    "$APP/ui/bananads-lan" $ui_arg
    rc=$?
    # 76: a game picked in the list; when it ends the list opens again on it (--games)
    if [ "$rc" = 76 ] && [ -f "$PLAY" ]; then
        play "$(head -n 1 "$PLAY")"
        ui_arg=--games
        continue
    fi
    break
done
# 75: the app has just updated itself (its folder is the new version now): open the new one
if [ "$rc" = 75 ] && [ -f "$APP/tile.sh" ]; then
    exec sh "$APP/tile.sh"
fi
exit "$rc"

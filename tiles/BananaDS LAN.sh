#!/bin/sh
# Apps shelf tile. bash runs this (run-store-app), so this file needs no +x.
# One folder too deep (store/apps/bananads-lan/bananads-lan/) still opens.
ROOT=${BANANADS_MP_APPS:-/data/store/apps}
APP=$ROOT/bananads-lan
if [ ! -f "$APP/tile.sh" ] && [ -f "$APP/bananads-lan/tile.sh" ]; then
    exec sh "$APP/bananads-lan/tile.sh"
fi
exec sh "$APP/tile.sh"

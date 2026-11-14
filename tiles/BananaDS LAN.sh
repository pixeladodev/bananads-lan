#!/bin/sh
# Apps shelf tile. bash runs this (run-store-app), so this file needs no +x.
ROOT=${BANANADS_MP_APPS:-/data/store/apps}
exec sh "$ROOT/bananads-lan/tile.sh"
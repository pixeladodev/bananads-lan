#!/bin/sh
# Scrollbar gestures: center on track tap and stepped thumb drag from the finger position at touch-down.
set -eu

center_list() {
    gsel=$1 ngames=$2
    half=$((GAME_ROWS / 2))
    top=$((gsel - half))
    [ "$top" -lt 0 ] && top=0
    maxt=$((ngames - GAME_ROWS))
    [ "$ngames" -gt "$GAME_ROWS" ] && [ "$top" -gt "$maxt" ] && top=$maxt
    echo "$top"
}

center_grid_col() {
    col=$1 cols=$2
    half=$((GRID_COLS / 2))
    left=$((col - half))
    [ "$left" -lt 0 ] && left=0
    maxl=$((cols - GRID_COLS))
    [ "$cols" -gt "$GRID_COLS" ] && [ "$left" -gt "$maxl" ] && left=$maxl
    echo "$left"
}

GAME_ROWS=6 GRID_COLS=5 GRID_ROWS=3

# Track tap: game 50 of 100 -> row 3 centered (top 47)
t=$(center_list 50 100)
[ "$t" -eq 47 ] || { echo "FAIL list center: top=$t want 47"; exit 1; }

# Pin start: game 1 -> top 0
t=$(center_list 1 100)
[ "$t" -eq 0 ] || { echo "FAIL list pin start: top=$t"; exit 1; }

# Pin end: game 99 -> top 94
t=$(center_list 99 100)
[ "$t" -eq 94 ] || { echo "FAIL list pin end: top=$t want 94"; exit 1; }

# Grid: col 10 in 30 columns -> left 8
l=$(center_grid_col 10 30)
[ "$l" -eq 8 ] || { echo "FAIL grid center: left=$l want 8"; exit 1; }

# Thumb drag: gsel0=20, span=400, delta 40 px -> +9 items (99 games, integer math as main.c)
ngames=99 span=400 delta=40 gsel0=20
steps=$((delta * (ngames - 1) / span))
g=$((gsel0 + steps))
[ "$g" -eq 29 ] || { echo "FAIL thumb step: g=$g want 29"; exit 1; }

# Release without move: threshold 10 px, delta 5 -> no step flag
dx=5 dy=0 thresh=10
moved=0
[ "$dx" -lt "$thresh" ] && [ "$dy" -lt "$thresh" ] || moved=1
[ "$moved" -eq 0 ] || { echo "FAIL no-move release"; exit 1; }

echo "OK rail-verify: list/grid centering and thumb step math"
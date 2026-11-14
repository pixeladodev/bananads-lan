#!/bin/sh
# Library::sortKey order: lower(sort or clean title), strcmp — not the rom basename with ".nds"
set -eu

lower() { printf '%s' "$1" | tr '[:upper:]' '[:lower:]'; }

# Old basename strcasecmp put "Bomberman 2.nds" before "Bomberman.nds" (space < '.').
bad() {
  a=$(lower "Bomberman.nds")
  b=$(lower "Bomberman 2.nds")
  case "$a" in "$b"|*) ;; *) [ "$a" \< "$b" ] 2>/dev/null && return 0 ;; esac
  [ "$b" \< "$a" ] 2>/dev/null
}

want_order() {
  for t in "$@"; do
    lower "$t"
  done | awk 'NR>1 && prev!="" && $0 < prev { exit 1 } { prev=$0 } END { exit 0 }'
}

if bad; then
  echo "OK sort-verify: basename compare shows the Bomberman regression"
else
  echo "WARN sort-verify: unexpected basename order"
fi

want_order "Bomberman" "Bomberman 2" "Bomberman Land Touch!" "Bomberman Land Touch! 2" \
  || { echo "FAIL sort key lex order"; exit 1; }

echo "OK sort-verify: BananaOS sortKey lexicographic order"
#!/bin/sh
# Apps shelf entry. The menu has already let go of both panels.
# sh, not a direct exec: a copy over SMB can drop the executable bit on scripts.
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$APP/common.sh"
mp_clean_junk
chmod a+x "$APP/ui/bananads-lan" "$APP/apply.sh" "$APP/revert.sh" "$APP/status.sh" \
    "$APP/run-nds-lan" "$APP/uninstall.sh" "$APP/fix-nest.sh" 2>/dev/null || true
exec "$APP/ui/bananads-lan"

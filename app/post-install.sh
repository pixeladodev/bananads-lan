#!/bin/sh
# After an install or an update, from the Store (its post_install) or from the app's own updater.
# An update from a version before 0.4.0: its mount over /usr/bin/bananads (the old folder is bananads-lan.old by
# now, or gone) and its launcher in systems.conf come off, so BananaOS's BananaDS is back as it came. The games the
# app opens run on emu/bananads instead.
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$APP/common.sh"
mp_clean_junk
chmod a+x "$APP/emu/bananads" "$APP/run-emu" 2>/dev/null || true
mp_legacy_cleanup

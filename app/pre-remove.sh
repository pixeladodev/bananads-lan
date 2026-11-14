#!/bin/sh
# Before the Store removes the app: anything an older version left behind comes off (a mount over
# /usr/bin/bananads, its launcher in systems.conf). The app's settings in bananads.conf stay; BananaOS's BananaDS
# doesn't read them.
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$APP/common.sh"
mp_legacy_cleanup

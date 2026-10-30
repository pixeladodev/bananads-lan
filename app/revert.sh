#!/bin/sh
# Turn BananaDS LAN off: drop the launcher and unmount this app's BananaDS.
set -eu
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$APP/common.sh"
mp_revert

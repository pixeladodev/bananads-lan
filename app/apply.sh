#!/bin/sh
# Turn BananaDS LAN on: record the launcher and mount this app's BananaDS.
set -eu
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$APP/common.sh"
mp_apply

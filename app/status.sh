#!/bin/sh
set -eu
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$APP/common.sh"
mp_status

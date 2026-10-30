#!/bin/sh
# Apps shelf entry. The menu has already let go of both panels.
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
exec "$APP/ui/bananads-lan"

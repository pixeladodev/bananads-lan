#!/bin/sh
# After an install or an update, from the Store (its post_install) or from the app's own updater.
# With local multiplayer on, the launcher line is written again and a BananaDS that was mounted is
# the new one: the old copy's mount (its folder is bananads-lan.old by now, or gone) comes off first.
# With it off, nothing changes.
APP=$(CDPATH= cd -- "$(dirname "$0")" && pwd)
. "$APP/common.sh"
mp_clean_junk
had=0
case $(mp_mount_root) in
    /store/apps/bananads-lan/bin/bananads* | /store/apps/bananads-lan.old/bin/bananads*)
        had=1
        umount -l /usr/bin/bananads || { echo "could not unmount the old BananaDS" >&2; exit 1; }
        ;;
esac
mp_enabled || exit 0
if ! mp_check; then
    # still On: a game then runs the stock BananaDS, and the app says why
    echo "$MP_REASON" >&2
    exit 0
fi
mp_install_hook || { echo "could not update $MP_CONF" >&2; exit 1; }
mp_env_clear || true
if [ "$had" = 1 ]; then
    mp_mount || { echo "could not mount BananaDS" >&2; exit 1; }
fi
exit 0

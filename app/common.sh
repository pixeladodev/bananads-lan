# Shared by status.sh, lan.sh, and run-emu. Busybox ash.
# The runtime directory is $APP, set by the caller (emu/bananads lives here).
# Overrides for a local check, never set on a handheld:
#   BANANADS_MP_CONF BANANADS_MP_ENV BANANADS_MP_OS BANANADS_MP_MODEL BANANADS_MP_BDCONF BANANADS_MP_NOMOUNT

MP_CONF=${BANANADS_MP_CONF:-/data/banana/systems.conf}
MP_ENV=${BANANADS_MP_ENV:-/data/banana/bananads.env}
MP_OS=${BANANADS_MP_OS:-/etc/os-release}
MP_MODEL_FILE=${BANANADS_MP_MODEL:-/proc/device-tree/model}
MP_BDCONF=${BANANADS_MP_BDCONF:-/data/bananads/bananads.conf}
MP_MARK_A="# bananads-lan start"
MP_MARK_B="# bananads-lan end"
MP_REASON=

mp_manifest() {
    sed -n "s/^$1=//p" "$APP/manifest" | head -n 1 | tr -d '\r'
}

mp_os_version() {
    sed -n 's/^VERSION_ID=//p' "$MP_OS" 2>/dev/null | head -n 1 | tr -d '"\r'
}

mp_model_id() {
    m=$(tr -d '\0' < "$MP_MODEL_FILE" 2>/dev/null || true)
    case $m in
        *Plus*) printf '%s' rgds_plus ;;
        "Anbernic RG DS") printf '%s' rgds ;;
        *) printf '%s' unknown ;;
    esac
}

# ---- the app's own BananaDS ----

# Ready when emu/bananads is there (run-emu makes it executable). Any BananaOS: nothing is checked against the
# system's BananaDS any more.
mp_check() {
    MP_REASON=
    if [ ! -s "$APP/emu/bananads" ]; then
        MP_REASON="install the app again"
        return 1
    fi
    return 0
}

# Local multiplayer: the mode our BananaDS starts a game in, wifi.local in bananads.conf (the in-game menu's
# Local multiplayer page writes the same key; BananaOS's BananaDS, built without LAN, never reads it). No saved
# value is Auto, BananaDS's default. On = anything but off.
mp_lan_mode() {
    sed -n 's/^ *wifi\.local *= *\([^ \t\r]*\).*/\1/p' "$MP_BDCONF" 2>/dev/null | tail -n 1
}

mp_enabled() {
    [ "$(mp_lan_mode)" != off ]
}

# mp_lan_set <value>: wifi.local = <value> in bananads.conf, the other lines as they are (written beside it and
# renamed: a full card never leaves half a file)
mp_lan_set() {
    d=$(dirname "$MP_BDCONF")
    mkdir -p "$d" || return 1
    tmp="$MP_BDCONF.tmp.$$"
    if [ -f "$MP_BDCONF" ]; then
        awk -v v="$1" '
            /^ *wifi\.local *=/ { if (!done) print "wifi.local = " v; done = 1; next }
            { print }
            END { if (!done) print "wifi.local = " v }
        ' "$MP_BDCONF" > "$tmp" || { rm -f "$tmp"; return 1; }
    else
        printf 'wifi.local = %s\n' "$1" > "$tmp" || { rm -f "$tmp"; return 1; }
    fi
    mv -f "$tmp" "$MP_BDCONF"
}

# ---- what versions before 0.4.0 left behind ----
# They mounted bin/bananads over /usr/bin/bananads and put an [nds] launcher between markers in systems.conf (and
# once a block in bananads.env). All of it comes off: the mount (the app's folder may be bananads-lan.old by now,
# during an update, or deleted), the markers' blocks, the old wrapper.

mp_unmark() {
    file=$1
    [ -f "$file" ] || return 0
    grep -qxF "$MP_MARK_A" "$file" || return 0
    tmp="$file.tmp.$$"
    awk -v a="$MP_MARK_A" -v b="$MP_MARK_B" '
        $0 == a { skip = 1; next }
        $0 == b { skip = 0; next }
        skip { next }
        { print }
    ' "$file" > "$tmp" || return 1
    mv -f "$tmp" "$file"
}

# Path inside the data partition of whatever is mounted on /usr/bin/bananads (empty if nothing is).
mp_mount_root() {
    awk '$5 == "/usr/bin/bananads" { print $4; exit }' /proc/self/mountinfo 2>/dev/null || true
}

mp_legacy_mounted() {
    [ "${BANANADS_MP_NOMOUNT:-}" = 1 ] && return 1
    case $(mp_mount_root) in
        /store/apps/bananads-lan/* | /store/apps/bananads-lan.old/* | /store/apps/bananads-lan.new/*) return 0 ;;
    esac
    return 1
}

mp_legacy_cleanup() {
    rc=0
    i=0
    # (a mount over a mount: each comes off; another app's binary mounted there is left alone)
    while mp_legacy_mounted && [ $i -lt 4 ]; do
        umount -l /usr/bin/bananads || { echo "could not unmount the old BananaDS LAN binary" >&2; rc=1; break; }
        i=$((i + 1))
    done
    mp_unmark "$MP_CONF" || { echo "could not update $MP_CONF" >&2; rc=1; }
    mp_unmark "$MP_ENV" || { echo "could not update $MP_ENV" >&2; rc=1; }
    [ -n "${APP:-}" ] && rm -rf "$APP/bin" "$APP/run-nds-lan" "$APP/apply.sh" "$APP/revert.sh" "$APP/.mounted" 2>/dev/null
    return $rc
}

# Drop the AppleDouble and Finder files a Mac copy leaves inside this app.
mp_clean_junk() {
    tiles=${BANANADS_MP_TILES:-/data/store/tiles}
    [ -n "${APP:-}" ] && [ -d "$APP" ] || return 0
    find "$APP" \( -name '._*' -o -name '.DS_Store' -o -name '__MACOSX' \) -exec rm -rf {} + 2>/dev/null || true
    apps=$(dirname "$APP")
    rm -f "$apps/._bananads-lan" "$apps/.DS_Store" 2>/dev/null || true
    if [ -d "$tiles" ]; then
        rm -f "$tiles/._BananaDS LAN.sh" "$tiles/.DS_Store" 2>/dev/null || true
    fi
}

mp_status() {
    if mp_check; then s=yes; else s=no; fi
    if mp_enabled; then e=yes; else e=no; fi
    if mp_legacy_mounted; then m=yes; else m=no; fi
    mode=$(mp_lan_mode)
    printf 'supported=%s\n' "$s"
    printf 'reason=%s\n' "$MP_REASON"
    printf 'enabled=%s\n' "$e"
    printf 'mode=%s\n' "${mode:-auto}"
    printf 'mounted=%s\n' "$m"
    printf 'os=%s\n' "$(mp_os_version)"
    printf 'version=%s\n' "$(mp_manifest app_version)"
    printf 'model=%s\n' "$(mp_model_id)"
}

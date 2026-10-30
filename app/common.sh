# Shared by apply.sh, revert.sh, run-nds-lan and status.sh. Busybox ash.
# The app directory is $APP, set by the caller.
# Overrides for a local check, never set on a handheld:
#   BANANADS_MP_CONF BANANADS_MP_OS BANANADS_MP_MODEL BANANADS_MP_STOCK_FILE BANANADS_MP_NOMOUNT

MP_CONF=${BANANADS_MP_CONF:-/data/banana/systems.conf}
MP_ENV=${BANANADS_MP_ENV:-/data/banana/bananads.env}
MP_OS=${BANANADS_MP_OS:-/etc/os-release}
MP_MODEL_FILE=${BANANADS_MP_MODEL:-/proc/device-tree/model}
MP_STOCK=${BANANADS_MP_STOCK:-/usr/bin/bananads}
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

mp_sha() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" 2>/dev/null | awk '{print $1; exit}'
    else
        shasum -a 256 "$1" 2>/dev/null | awk '{print $1; exit}'
    fi
}

# The BananaDS that came with the OS, even when another file is mounted on top of it.
mp_stock_sha() {
    if [ -n "${BANANADS_MP_STOCK_FILE:-}" ]; then
        mp_sha "$BANANADS_MP_STOCK_FILE"
        return
    fi
    if awk '$5 == "/usr/bin/bananads" { found = 1 } END { exit !found }' /proc/self/mountinfo 2>/dev/null; then
        d=$(mktemp -d) || return 1
        if mount --bind /usr "$d"; then
            mp_sha "$d/bin/bananads"
            umount "$d" || true
        fi
        rmdir "$d" 2>/dev/null || true
        return
    fi
    mp_sha "$MP_STOCK"
}

# Path inside the data partition of whatever is mounted on /usr/bin/bananads (empty if nothing is).
mp_mount_root() {
    awk '$5 == "/usr/bin/bananads" { print $4; exit }' /proc/self/mountinfo 2>/dev/null || true
}

mp_ours() {
    if [ "${BANANADS_MP_NOMOUNT:-}" = 1 ]; then
        [ -f "$APP/.mounted" ] && return 0
        return 1
    fi
    case $(mp_mount_root) in
        /store/apps/bananads-lan/bin/bananads | /store/apps/bananads-lan/bin/bananads//deleted) return 0 ;;
    esac
    return 1
}

mp_foreign_mount() {
    if [ "${BANANADS_MP_NOMOUNT:-}" = 1 ]; then
        return 1
    fi
    r=$(mp_mount_root)
    [ -n "$r" ] || return 1
    mp_ours && return 1
    return 0
}

mp_check() {
    MP_REASON=
    want=$(mp_manifest bananaos)
    got=$(mp_os_version)
    if [ -z "$got" ] || [ "$got" != "$want" ]; then
        MP_REASON="BananaOS ${got:-unknown} is not supported (this app is for $want)"
        return 1
    fi
    id=$(mp_model_id)
    case $id in
        rgds|rgds_plus) ;;
        *) MP_REASON="this handheld is not an RG DS or RG DS Plus"; return 1 ;;
    esac
    expect=$(mp_manifest "stock_sha_$id")
    if [ -z "$expect" ]; then
        MP_REASON="the stock BananaDS hash for this handheld is not in the manifest yet"
        return 1
    fi
    stock=$(mp_stock_sha)
    if [ "$stock" != "$expect" ]; then
        MP_REASON="stock BananaDS does not match this BananaOS build"
        return 1
    fi
    bin=$(mp_sha "$APP/bin/bananads")
    wantb=$(mp_manifest binary_sha)
    if [ -z "$bin" ] || [ "$bin" != "$wantb" ]; then
        MP_REASON="the app's BananaDS binary is missing or does not match the manifest"
        return 1
    fi
    return 0
}

mp_enabled() {
    [ -f "$MP_CONF" ] || return 1
    grep -qxF "$MP_MARK_A" "$MP_CONF"
}

mp_install_hook() {
    # Rewrite the block on every On, so a newer app replaces the launcher line.
    mp_unmark "$MP_CONF" || return 1
    d=$(dirname "$MP_CONF")
    mkdir -p "$d" || return 1
    tmp="$MP_CONF.tmp.$$"
    if [ -f "$MP_CONF" ]; then
        cat "$MP_CONF" > "$tmp" || return 1
        # a newline before the block when the file does not already end with one
        tail -c 1 "$MP_CONF" 2>/dev/null | grep -q '^$' || printf '\n' >> "$tmp"
    else
        : > "$tmp"
    fi
    cat >> "$tmp" << EOF
$MP_MARK_A
[nds]
run = if [ -f /data/store/apps/bananads-lan/run-nds-lan ]; then exec /data/store/apps/bananads-lan/run-nds-lan %ROM%; fi; case \$(awk '\$5=="/usr/bin/bananads" { print \$4; exit }' /proc/self/mountinfo) in /store/apps/bananads-lan/bin/bananads | /store/apps/bananads-lan/bin/bananads//deleted) umount -l /usr/bin/bananads ;; esac; exec /usr/libexec/banana/run-nds %ROM%
$MP_MARK_B
EOF
    mv -f "$tmp" "$MP_CONF"
}

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

mp_remove_hook() {
    mp_unmark "$MP_CONF"
}

# Drop a marked block if an older On left BANANADS_LINK_SCRIPT or BANANADS_LAN_EARLY_REPLY here.
# This build does not read them.
mp_env_clear() {
    mp_unmark "$MP_ENV"
}

mp_mount() {
    if [ "${BANANADS_MP_NOMOUNT:-}" = 1 ]; then
        : > "$APP/.mounted"
        return 0
    fi
    mp_ours && return 0
    chmod a+x "$APP/bin/bananads" 2>/dev/null || true
    mount --bind "$APP/bin/bananads" /usr/bin/bananads
}

mp_umount() {
    if [ "${BANANADS_MP_NOMOUNT:-}" = 1 ]; then
        rm -f "$APP/.mounted"
        return 0
    fi
    mp_ours || return 0
    umount -l /usr/bin/bananads
}

mp_apply() {
    if ! mp_check; then
        echo "$MP_REASON" >&2
        return 1
    fi
    if mp_foreign_mount; then
        echo "another binary is mounted on /usr/bin/bananads; unmount it, then turn this on" >&2
        return 1
    fi
    mp_install_hook || { echo "could not update $MP_CONF" >&2; return 1; }
    mp_env_clear || { echo "could not update $MP_ENV" >&2; return 1; }
    mp_mount || { echo "could not mount BananaDS" >&2; return 1; }
}

mp_revert() {
    mp_remove_hook || { echo "could not update $MP_CONF" >&2; return 1; }
    mp_env_clear || { echo "could not update $MP_ENV" >&2; return 1; }
    if mp_foreign_mount; then
        echo "left the other binary mounted on /usr/bin/bananads" >&2
        return 0
    fi
    mp_umount || { echo "could not unmount BananaDS" >&2; return 1; }
}

# Game launch. The menu keeps the launcher it read at startup, so after Off it can still call this
# wrapper. Mount only while On. Off, drop our mount so this launch is the OS BananaDS.
mp_prepare_game() {
    if mp_enabled && mp_check; then
        if mp_foreign_mount; then
            echo "bananads-lan: another binary is mounted on /usr/bin/bananads; not replacing it" >&2
        else
            mp_mount || echo "bananads-lan: could not mount BananaDS; the stock one runs" >&2
        fi
    else
        if ! mp_check; then
            echo "bananads-lan: $MP_REASON; the stock BananaDS runs" >&2
        fi
        if mp_ours; then mp_umount || true; fi
    fi
}

mp_emulator() {
    f=${BANANADS_MP_SETTINGS:-/data/banana/settings.conf}
    sed -n 's/^ *nds_emulator *= *//p' "$f" 2>/dev/null | head -n 1 | tr -d '\r' || true
}

mp_status() {
    if mp_check; then s=yes; else s=no; fi
    if mp_enabled; then e=yes; else e=no; fi
    if mp_ours; then m=yes; else m=no; fi
    emu=$(mp_emulator)
    [ -n "$emu" ] || emu=unset
    printf 'supported=%s\n' "$s"
    printf 'reason=%s\n' "$MP_REASON"
    printf 'enabled=%s\n' "$e"
    printf 'mounted=%s\n' "$m"
    printf 'emulator=%s\n' "$emu"
    printf 'os=%s\n' "$(mp_os_version)"
    printf 'version=%s\n' "$(mp_manifest app_version)"
    printf 'model=%s\n' "$(mp_model_id)"
}

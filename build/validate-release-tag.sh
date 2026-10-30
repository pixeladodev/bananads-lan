#!/bin/bash
# On a tag push, the tag must match app_version (v0.1.0 -> 0.1.0).
set -euo pipefail
tag=${1:-${GITHUB_REF_NAME:-}}
[ -n "$tag" ] || { echo "validate-release-tag: no tag" >&2; exit 1; }
case $tag in v*) ver=${tag#v} ;; *) echo "validate-release-tag: tag must start with v (got $tag)" >&2; exit 1 ;; esac
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
want=$(sed -n 's/^app_version=//p' "$ROOT/app/manifest" | head -n 1 | tr -d '\r')
[ "$ver" = "$want" ] || {
    echo "validate-release-tag: tag $tag is version $ver but app/manifest has app_version=$want" >&2
    echo "Use tag v${want} for this release." >&2
    exit 1
}
echo "validate-release-tag: ok ($tag matches app_version=$want)"

#!/usr/bin/env bash
# Copies Argon's patches from the dev tree back into this repo. Run `he unmerge` in the tree first.
# Usage: dev/pull-patches.sh [tree]   (default: $ARGON_TREE or ~/argon-tree)
# New patches must be created under argon/ (`quilt new argon/ui/<name>.patch`) to be picked up. New files of Argon's
# own go in argon/src.patch (`quilt push argon/src.patch`, `quilt add`, then push the rest back).
set -euo pipefail

argon=$(cd "$(dirname "$0")/.." && pwd)
tree=${1:-${ARGON_TREE:-$HOME/argon-tree}}

if [ -f "$tree/patches/series.merged" ]; then
  echo "Patches are still merged: run 'he unmerge' in $tree first." >&2
  exit 1
fi

rsync -a --delete --exclude src.patch "$tree/patches/argon/" "$argon/patches/argon/"
{ head -1 "$argon/patches/series"; grep '^argon/' "$tree/patches/series" | grep -vx argon/src.patch; } \
  > "$argon/patches/series.new"
mv "$argon/patches/series.new" "$argon/patches/series"
# argon/src.patch (made by ci/src-patch.sh) back into plain files.
rm -rf "$argon/src" && mkdir "$argon/src"
patch -d "$argon/src" -p1 -s < "$tree/patches/argon/src.patch"
git -C "$argon" status --short patches src

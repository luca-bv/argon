#!/usr/bin/env bash
# Copies Argon's patches from the dev tree back into this repo. Run `he unmerge` in the tree first.
# Usage: dev/pull-patches.sh [tree]   (default: $ARGON_TREE or ~/argon-tree)
# New patches must be created under argon/ (`quilt new argon/ui/<name>.patch`) to be picked up.
set -euo pipefail

argon=$(cd "$(dirname "$0")/.." && pwd)
tree=${1:-${ARGON_TREE:-$HOME/argon-tree}}

if [ -f "$tree/patches/series.merged" ]; then
  echo "Patches are still merged: run 'he unmerge' in $tree first." >&2
  exit 1
fi

rsync -a --delete "$tree/patches/argon/" "$argon/patches/argon/"
{ head -1 "$argon/patches/series"; grep '^argon/' "$tree/patches/series"; } > "$argon/patches/series.new"
mv "$argon/patches/series.new" "$argon/patches/series"
git -C "$argon" status --short patches

#!/usr/bin/env bash
# Usage: ci/assemble.sh <helium-macos tag> <out dir>
# Clones helium-macos at <tag> and layers Argon on top: CI tweaks, then patches/series appended to Helium's.
set -euo pipefail

tag=$1 out=$2
argon=$(cd "$(dirname "$0")/.." && pwd)

git clone -q --depth 1 --branch "$tag" --recurse-submodules --shallow-submodules \
  https://github.com/imputnet/helium-macos "$out"
cd "$out"

git apply "$argon/ci/helium-macos.patch"

# Stock Chromium interface (README "Interface base"): drop Helium's UI patches and the patches that need them.
drop() {  # drop <series> <patch>...  Fails if Helium renamed or removed one, so the list can't go stale quietly.
  local series=$1; shift
  for p; do grep -qxF "$p" "$series" || { echo "assemble: $p is not in $series" >&2; exit 1; }; done
  grep -vxF -f <(printf '%s\n' "$@") "$series" > "$series.new"
  mv "$series.new" "$series"
}
core=helium-chromium/patches/series
drop "$core" $(grep '^helium/ui/' "$core") \
  helium/core/custom-keyboard-shortcuts.patch helium/settings/custom-keyboard-shortcuts-page.patch
drop patches/series helium/macos/{helium-immersive-fullscreen,disable-chromium-immersive-fullscreen}.patch \
  helium/macos/native-frame-materials.patch helium/macos/move-macos-settings.patch
# CI commits this tree, and a submodule would be committed as a bare pointer, losing the edit above: keep plain files.
git rm -q --cached helium-chromium .gitmodules
rm -rf helium-chromium/.git .gitmodules

cp -R "$argon/patches/argon" patches/
[ -z "$(tail -c1 patches/series)" ] || echo >> patches/series  # no blank line: `he validate series` rejects them
cat "$argon/patches/series" >> patches/series

# Helium's own workflows don't run here (Argon's do), and GITHUB_TOKEN can't push workflow files.
rm -rf .github/workflows

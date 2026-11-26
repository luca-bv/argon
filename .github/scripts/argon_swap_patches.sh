#!/usr/bin/env bash
# Usage: swap-patches.sh <old patches dir> <new patches dir> <src dir>
# Each patches dir holds a series and argon/. Turns a source tree that has the old Argon patches applied into one
# with the new ones, so a fixed build can resume from a failed run's build directory. Files that end up unchanged
# keep their timestamps, so the build doesn't recompile them (a patched widely included header would mean hours).
set -euo pipefail

old=$(cd "$1" && pwd) new=$(cd "$2" && pwd)
cd "$3"

argon() { grep '^argon/' "$1/series"; }
touched() { for p in $(argon "$1"); do sed -n 's|^+++ b/||p' "$1/$p"; done; }

keep=$(mktemp -d)
for f in $( { touched "$old"; touched "$new"; } | sort -u); do
  if [ -e "$f" ]; then mkdir -p "$keep/$(dirname "$f")"; cp -p "$f" "$keep/$f"; fi
done

for p in $(argon "$old" | awk '{l[NR] = $0} END {for (i = NR; i; i--) print l[i]}'); do
  patch -R -E -p1 --ignore-whitespace --fuzz=0 --no-backup-if-mismatch -s < "$old/$p"
done
for p in $(argon "$new"); do
  patch -p1 --ignore-whitespace --fuzz=0 --forward --no-backup-if-mismatch -s < "$new/$p"
done

(cd "$keep" && find . -type f) | while read -r f; do
  if cmp -s "$keep/$f" "$f"; then touch -r "$keep/$f" "$f"; else echo "swap-patches: changed $f"; fi
done
rm -rf "$keep"

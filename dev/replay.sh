#!/usr/bin/env bash
# Usage: dev/replay.sh <assembled tree> <pristine files> <work dir>
# Applies the assembled series (Helium's, then Argon's) the way Helium's patches.py does, to a partial copy of
# Chromium's source, and keeps going past failures. Use it to check that patches apply, and to get the
# post-Helium copy of an upstream file to write an Argon hook patch against (Helium patches many of them).
#   ci/assemble.sh 0.22.2.1 /tmp/tree                      # the assembled tree
#   grep -h '^+++ b/' /tmp/tree/helium-chromium/patches/**/*.patch /tmp/tree/patches/**/*.patch \
#     | sed 's|^+++ b/||' | sort -u | xargs dev/fetch-chromium.sh /tmp/pristine
#   dev/replay.sh /tmp/tree /tmp/pristine /tmp/work         # /tmp/work/<path> is the post-Helium file
# Failures in uBlock's and Sparkle's files and in devtools-frontend are expected (separate downloads).
set -u
tree=$1 src=$2 work=$3
rm -rf "$work"; mkdir -p "$work"; cp -R "$src/." "$work/"
ok=0 bad=0
for set in "$tree/helium-chromium/patches" "$tree/patches"; do
  while read -r p; do
    case "$p" in ''|'#'*) continue;; esac
    if out=$(/usr/bin/patch -p1 --ignore-whitespace --fuzz=0 --forward --no-backup-if-mismatch -d "$work" -i "$set/$p" </dev/null 2>&1); then
      ok=$((ok+1))
    else
      bad=$((bad+1)); echo "FAIL $p"; echo "$out" | grep -i -B1 'fail\|ignored\|not found\|reject\|File to patch' | head -6 | sed 's/^/    /'
    fi
  done < "$set/series"
done
echo "applied $ok, failed $bad"

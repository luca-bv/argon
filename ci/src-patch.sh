#!/usr/bin/env bash
# Usage: ci/src-patch.sh <src dir>
# Prints a patch that creates every file under <src dir> (Argon's own code, laid out like the Chromium tree), so the
# build applies it like any other patch. dev/pull-patches.sh turns it back into files.
set -euo pipefail
cd "$1"
find . -type f ! -name .DS_Store | sed 's|^\./||' | LC_ALL=C sort | while read -r f; do
  [ -s "$f" ] && [ -z "$(tail -c1 "$f")" ] || { echo "src-patch: $f is empty or has no final newline" >&2; exit 1; }
  n=$(wc -l < "$f" | tr -d ' ')
  printf -- '--- /dev/null\n+++ b/%s\n@@ -0,0 +1,%s @@\n' "$f" "$n"
  sed 's/^/+/' "$f"
done

#!/usr/bin/env bash
# Usage: dev/fetch-chromium.sh <out dir> <chromium path>...
# Fetches single files of Chromium's source at $CHROMIUM_TAG (default 158.0.8127.36, Helium 0.22.2) from
# googlesource, to read upstream code or write a patch without a checkout. A file that moved or doesn't exist
# prints "fail <path>" (googlesource answers 404); devtools-frontend lives in its own repository.
set -u
out=$1; shift
tag=${CHROMIUM_TAG:-158.0.8127.36}
for p; do
  mkdir -p "$out/$(dirname "$p")"
  [ -s "$out/$p" ] && continue
  curl -sf "https://chromium.googlesource.com/chromium/src/+/refs/tags/$tag/$p?format=TEXT" | base64 -d > "$out/$p" \
    && [ -s "$out/$p" ] || { rm -f "$out/$p"; echo "fail $p"; }
done

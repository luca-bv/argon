#!/usr/bin/env zsh
# Sets up a Helium + Argon dev tree on a fresh Mac (the rented EC2 Mac) and builds it once.
# Usage: dev/setup.sh [helium-macos tag]   (default: latest release). Takes hours: run it inside tmux.
# Tree: $ARGON_TREE, default ~/argon-tree. Requirements from helium-macos docs/building.md.
set -euo pipefail

argon=${0:A:h:h}
tree=${ARGON_TREE:-$HOME/argon-tree}
tag=${1:-$(curl -sf 'https://api.github.com/repos/imputnet/helium-macos/releases?per_page=1' |
  python3 -c 'import json,sys; print(json.load(sys.stdin)[0]["tag_name"])')}

ram=$(( $(sysctl -n hw.memsize) / 1024 ** 3 ))
free=$(df -g "$HOME" | awk 'NR==2 {print $4}')
(( ram >= 32 )) || echo "warn: ${ram} GB RAM, linking Chromium wants 32+" >&2
if (( free < 200 )); then
  echo "Only ${free} GB free; need 200+. Grow the EBS root volume in the AWS console, then (AWS's documented steps):" >&2
  echo '  PDISK=$(diskutil list physical external | head -n1 | cut -d" " -f1)' >&2
  echo '  APFSCONT=$(diskutil list physical external | grep Apple_APFS | tr -s " " | cut -d" " -f8)' >&2
  echo '  sudo diskutil repairDisk $PDISK && sudo diskutil apfs resizeContainer $APFSCONT 0' >&2
  exit 1
fi
if ! xcodebuild -version 2>/dev/null | grep -q '^Xcode 26'; then
  echo "Xcode 26 is required (full Xcode, not just the command line tools). Install it, open it once, then:" >&2
  echo "  sudo xcode-select --switch /Applications/Xcode.app" >&2
  exit 1
fi
command -v brew >/dev/null || { echo "Install Homebrew first: https://brew.sh" >&2; exit 1; }

brew install python@3.13 wget zstd coreutils readline quilt gnu-sed sccache tmux
brew unlink binutils 2>/dev/null || true
export PATH="$(brew --prefix python@3.13)/libexec/bin:$PATH"
pip3 install requests pillow --break-system-packages
xcodebuild -downloadComponent MetalToolchain

[ -d "$tree" ] || "$argon/ci/assemble.sh" "$tag" "$tree"
cd "$tree"
source ./dev.sh
he setup
he build
echo "Built. Run it with: cd $tree && source dev.sh && he run"

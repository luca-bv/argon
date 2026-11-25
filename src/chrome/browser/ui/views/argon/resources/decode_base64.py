#!/usr/bin/env python3
# Copyright 2026 The Argon Authors
# Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.
"""Usage: decode_base64.py <input.b64> <output>

Argon's own files reach the build as a text patch (ci/src-patch.sh), so binary
resources are stored base64-encoded and decoded at build time."""

import base64
import sys

with open(sys.argv[1], 'rb') as src, open(sys.argv[2], 'wb') as dst:
  dst.write(base64.b64decode(src.read()))

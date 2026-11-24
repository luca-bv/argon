// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_palette.h"

#include "ui/gfx/color_utils.h"

ArgonPalette::ArgonPalette(SkColor frame) {
  const bool dark = color_utils::IsDark(frame);
  ink = dark ? SkColorSetRGB(0xf1, 0xef, 0xe9) : SkColorSetRGB(0x1d, 0x1c, 0x1a);
  const auto over_frame = [&](float alpha) {
    return color_utils::AlphaBlend(ink, frame, alpha);
  };
  // The handoff gives exact light values; dark ones are ink over the frame.
  secondary = dark ? over_frame(0.72f) : SkColorSetRGB(0x5c, 0x59, 0x53);
  muted = dark ? over_frame(0.5f) : SkColorSetRGB(0x8a, 0x85, 0x7c);
  line = over_frame(0.1f);
  hover = over_frame(0.05f);
  active = over_frame(0.065f);
}

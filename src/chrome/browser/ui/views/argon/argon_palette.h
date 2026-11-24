// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_PALETTE_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_PALETTE_H_

#include "third_party/skia/include/core/SkColor.h"

// Argon's ink tokens (design_handoff_argon_browser/README.md, "Design
// tokens"), for things drawn on the window frame. They follow the frame color,
// so they fit Paper and Dark (chrome/browser/ui/color/argon_color_mixer.cc)
// and stay readable on a custom theme.
struct ArgonPalette {
  // Light tokens on Paper, the default before the view has a theme.
  ArgonPalette() : ArgonPalette(SkColorSetRGB(0xf3, 0xf1, 0xed)) {}
  explicit ArgonPalette(SkColor frame);

  SkColor ink;        // Active text.
  SkColor secondary;  // Tab titles.
  SkColor muted;      // Space name, "+ New", "Clear", close icons.
  SkColor line;       // Dividers.
  SkColor hover;      // Row background under the pointer.
  SkColor active;     // The active tab's row.
};

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_PALETTE_H_

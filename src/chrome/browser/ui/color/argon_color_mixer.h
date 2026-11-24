// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_ARGON_COLOR_MIXER_H_
#define CHROME_BROWSER_UI_COLOR_ARGON_COLOR_MIXER_H_

namespace ui {
class ColorProvider;
struct ColorProviderKey;
}  // namespace ui

// Argon's window tones (design_handoff_argon_browser/README.md, "Design
// tokens"): the frame in Paper (light) or Dark, and the toolbar in the page
// color, so the toolbar and page read as one card on the frame. Added after
// Chromium's own mixers and before a custom theme's, so themes still win.
void AddArgonColorMixer(ui::ColorProvider* provider,
                        const ui::ColorProviderKey& key);

#endif  // CHROME_BROWSER_UI_COLOR_ARGON_COLOR_MIXER_H_

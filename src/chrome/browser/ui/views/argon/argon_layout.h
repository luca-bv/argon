// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_LAYOUT_H_

#include <array>

// The parts of Argon's chrome that a layout places in a zone
// (design_handoff_argon_browser/README.md, "Modules"). The Customize page will
// let people rearrange them; each zone draws its list in order.
enum class ArgonModule {
  kNavigation,  // Back, forward, reload and the current site (opens ⌘L).
  kSpaceName,
  kDivider,  // Skipped when nothing is drawn between it and the last divider.
  kPinned,     // Global pins (Chromium-pinned tabs), like Arc's Favorites.
  kSpacePins,  // The space's own pins (argon_pins.h).
  kLoose,      // Temporary tabs.
  kNewTab,   // "+ New", with "Clear" while there are loose tabs.
  kSpaces,   // One button per space, and "+" for a new one.
};

// ponytail: the "Quiet" preset is the only layout until the Customize page
// stores one in the profile's prefs.
inline constexpr auto kQuietSidebar = std::to_array<ArgonModule>({
    ArgonModule::kNavigation,
    ArgonModule::kSpaceName,
    ArgonModule::kDivider,
    ArgonModule::kPinned,
    ArgonModule::kDivider,
    ArgonModule::kSpacePins,
    ArgonModule::kDivider,
    ArgonModule::kLoose,
    ArgonModule::kNewTab,
    ArgonModule::kDivider,
    ArgonModule::kSpaces,
});

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_LAYOUT_H_

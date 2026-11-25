// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_PREFS_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_PREFS_H_

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "ui/color/color_provider_key.h"

class PrefRegistrySimple;
class PrefService;

// The parts of Argon's chrome that a layout places in a zone
// (design_handoff_argon_browser/README.md, "Modules"). Each zone draws its
// list in order.
enum class ArgonModule {
  kNavigation,  // "nav": back, forward, reload and the site (opens ⌘L).
  kSpaceName,   // "spaceName"
  kDivider,  // "divider": skipped when nothing is drawn since the last one.
  kPinned,     // "favorites": global pins (Chromium-pinned tabs).
  kSpacePins,  // "pinned": the space's own pins (argon_pins.h).
  kLoose,      // "loose": temporary tabs.
  kNewTab,     // "newTab": "+ New", with "Clear" while there are loose tabs.
  kSpaces,     // "spaces": one button per space, and "+" for a new one.
};

namespace argon {

// Layout and behaviour, global (Local State): the handoff's Customize state,
// { "zones": { "side": [...], "top": [...] }, "cfg": { ... } }, spelled as in
// `Argon Customize.dc.html`. Empty by default; every key has its default here,
// and keys this build doesn't use are kept as stored.
inline constexpr char kCustomizePref[] = "argon.customize";
// Colors, per profile: "paper", "cool" or "tinted". Dark and light come from
// Chromium's own per-profile mode (prefs::kBrowserColorScheme), so the default
// is Paper in light mode and Dark in dark mode.
inline constexpr char kTonePref[] = "argon.tone";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);
void RegisterProfilePrefs(PrefRegistrySimple* registry);

enum class Density { kCompact, kRegular, kRoomy };
enum class TabIcon { kTile, kDot, kNone };

struct Settings {
  Settings();
  Settings(const Settings&);
  Settings& operator=(const Settings&);
  ~Settings();

  // From Local State (defaults if there is none, as in tests).
  static Settings Load();

  std::vector<ArgonModule> side;
  Density density = Density::kRegular;
  TabIcon icon = TabIcon::kTile;
  // false: loose tabs never show ×.
  bool close_on_hover = true;
  // nullopt: never archive.
  std::optional<base::TimeDelta> archive_after = base::Hours(12);
  bool edge_pill = true;
  // The page card's inset from the frame and its corner radius.
  int gap = 8;
  int radius = 10;
};

// Settings::Load().gap / .radius, for the frame's layout.
int PageGap();
int PageRadius();

// Puts the profile's tone into a window's color key
// (ThemeService::GetColorProviderKey, argon/ui/colors.patch).
void ApplyTone(const PrefService& profile_prefs, ui::ColorProviderKey& key);

}  // namespace argon

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_PREFS_H_

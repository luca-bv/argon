// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_PINS_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_PINS_H_

#include <map>
#include <string>
#include <vector>

#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

// Arc's per-space pins (docs/notes.md, "Spaces", stage 2). A pin is a tab in
// its space's group with a pinned URL: drawn above the space's loose tabs,
// never archived, and closing it takes it back to its pinned URL instead.
// (Chromium-pinned tabs are the global pins, shown in every space; Helium's
// helium/core/pins/ handles those.) The pinned URL is kept on the WebContents
// and in the session's tab extra data, so it survives restarts;
// argon/ui/space-pins.patch hooks session restore, session rebuilds, reopening
// closed windows and user closes into this file.
namespace argon_pins {

inline constexpr char kPinUrlKey[] = "argon_pin_url";

// The tab's pinned URL, or an empty GURL if it isn't a per-space pin.
GURL PinnedUrl(content::WebContents* contents);
bool IsPin(content::WebContents* contents);
// Pins the tab at `url`, or unpins it when `url` is empty, and saves that to
// the session.
void SetPin(content::WebContents* contents, const GURL& url);

// Session hooks: state from a restored tab's extra data, and the extra data to
// save for a tab.
void RestoreFromExtraData(content::WebContents* contents,
                          const std::map<std::string, std::string>& extra_data);
std::map<std::string, std::string> ExtraData(content::WebContents* contents);

// User-gesture closes (⌘W, ×): pins go back to their pinned URL, or, if
// already there and active, hand activation to the space's last used tab.
// Returns the tabs that should close.
std::vector<tabs::TabHandle> HandleUserClosingTabs(
    BrowserWindowInterface* browser,
    std::vector<tabs::TabHandle> tab_handles);

}  // namespace argon_pins

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_PINS_H_

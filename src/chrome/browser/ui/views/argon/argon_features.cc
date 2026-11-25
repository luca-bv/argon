// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_features.h"

#include <set>

#include "base/no_destructor.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/sessions/core/session_id.h"

BASE_FEATURE(kArgonHideToolbar, base::FEATURE_ENABLED_BY_DEFAULT);

namespace {

// Windows whose toolbar was shown with "Show toolbar".
std::set<SessionID>& ShownToolbars() {
  static base::NoDestructor<std::set<SessionID>> shown;
  return *shown;
}

}  // namespace

bool ArgonToolbarHidden(const BrowserWindowInterface* browser) {
  return base::FeatureList::IsEnabled(kArgonHideToolbar) &&
         !ShownToolbars().contains(browser->GetSessionID());
}

void SetArgonToolbarShown(const BrowserWindowInterface* browser, bool shown) {
  if (shown) {
    ShownToolbars().insert(browser->GetSessionID());
  } else {
    ShownToolbars().erase(browser->GetSessionID());
  }
}

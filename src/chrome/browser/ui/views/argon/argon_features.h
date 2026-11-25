// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_FEATURES_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_FEATURES_H_

#include "base/feature_list.h"

class BrowserWindowInterface;

// With vertical tabs, hide Chromium's toolbar; the sidebar and the command bar
// replace it (argon/ui/no-toolbar.patch). --disable-features=ArgonHideToolbar
// brings it back, with extensions, the app menu and downloads.
BASE_DECLARE_FEATURE(kArgonHideToolbar);

// Whether `browser`'s toolbar is hidden: the feature is on and the command
// bar's "Show toolbar" hasn't brought it back in that window.
bool ArgonToolbarHidden(const BrowserWindowInterface* browser);
void SetArgonToolbarShown(const BrowserWindowInterface* browser, bool shown);

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_FEATURES_H_

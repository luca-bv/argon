// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_COMMAND_BAR_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_COMMAND_BAR_H_

class BrowserWindowInterface;

// Argon's command bar (design_handoff_argon_browser/README.md, "Command bar"):
// a field over the page that finds tabs in every space, opens a URL or a
// search, and goes to a space. Like Arc, ⌘T opens it to start a new tab, and
// ⌘L opens it on the current page's URL to replace it
// (argon/ui/command-bar.patch hooks IDC_NEW_TAB and IDC_FOCUS_LOCATION).
//
// Returns false, so the caller falls back to Chromium's own behavior, when the
// window isn't a normal browser window.
bool ShowArgonCommandBar(BrowserWindowInterface* browser, bool new_tab);

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_COMMAND_BAR_H_

// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_TOAST_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_TOAST_H_

#include <string>

#include "base/functional/callback_forward.h"

class BrowserWindowInterface;

// A toast at the bottom center of the window (design_handoff_argon_browser/
// README.md, "Toast / undo"): `text`, and an Undo button that runs `undo`.
// Shown for 6 seconds; a newer toast replaces it. It never takes focus.
void ShowArgonToast(BrowserWindowInterface* browser,
                    std::u16string text,
                    base::OnceClosure undo);

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_TOAST_H_

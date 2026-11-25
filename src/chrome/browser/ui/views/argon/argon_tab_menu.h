// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_TAB_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_TAB_MENU_H_

#include "base/memory/weak_ptr.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"

class BrowserWindowInterface;

namespace gfx {
class Point;
}  // namespace gfx

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace views {
class View;
}  // namespace views

// The menu on a sidebar tab row: pin it in its space or unpin it, take a pin
// back to its pinned URL or make the current URL its pinned URL, add it to or
// remove it from Favorites (global pins), close it.
void ShowArgonTabMenu(views::View* source,
                      BrowserWindowInterface* browser,
                      base::WeakPtr<tabs::TabInterface> tab,
                      const gfx::Point& point,
                      ui::mojom::MenuSourceType source_type);

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_TAB_MENU_H_

// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_SPACE_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_SPACE_MENU_H_

#include "chrome/browser/ui/views/argon/argon_window_set.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

// The menu on a space's button: which profile the space uses (its logins and
// history). Picking another profile moves the space into that profile's window
// (argon_window_set.h) by reopening its tabs there; "New profile" makes one
// named after the space first. Moving loses the tabs' back/forward history and
// form contents, and shows a toast whose Undo moves it back.
void ShowArgonSpaceMenu(views::View* source,
                        const ArgonWindowSet::Space& space,
                        const gfx::Point& point,
                        ui::mojom::MenuSourceType source_type);

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_SPACE_MENU_H_

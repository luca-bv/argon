// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_FONT_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_FONT_H_

#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"

// Argon's typeface, Allo (resources/, SIL Open Font License), bundled in the
// framework's Resources and registered for this process on first use.
// ponytail: gfx::FontList can't set OpenType features or variation axes, so
// the handoff's 'ss01'/'tnum' and ROND 100 aren't applied (ROND stays at the
// font's default, 50).
gfx::FontList ArgonFont(int size, gfx::Font::Weight weight);

// Makes Allo the default font of Chromium's own UI too (gfx::FontList()), at
// the Mac UI size. Called at startup once the resource bundle has set the
// default (argon/ui/allo-default-font.patch); views built from gfx::Font()
// directly keep the system font.
void UseArgonFontAsDefault();

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_FONT_H_

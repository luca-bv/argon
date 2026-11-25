// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_font.h"

#include <CoreText/CoreText.h>

#include <string>
#include <vector>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"

namespace {

// The family name in AlloVariable.ttf's name table.
constexpr char kFamily[] = "Allo Variable";

void RegisterOnce() {
  static const bool registered = [] {
    const base::FilePath path = base::apple::FrameworkBundlePath()
                                    .Append("Resources")
                                    .Append("AlloVariable.ttf");
    base::apple::ScopedCFTypeRef<CFURLRef> url =
        base::apple::FilePathToCFURL(path);
    // On failure, FontList falls back to the system font.
    return url && CTFontManagerRegisterFontsForURL(
                      url.get(), kCTFontManagerScopeProcess, nullptr);
  }();
  (void)registered;
}

}  // namespace

void UseArgonFontAsDefault() {
  RegisterOnce();
  gfx::FontList::SetDefaultFontDescription(std::string(kFamily) + ", 13px");
}

gfx::FontList ArgonFont(int size, gfx::Font::Weight weight) {
  RegisterOnce();
  return gfx::FontList(std::vector<std::string>{kFamily}, gfx::Font::NORMAL,
                       size, weight);
}

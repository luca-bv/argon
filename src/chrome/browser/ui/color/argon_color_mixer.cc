// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/color/argon_color_mixer.h"

#include "chrome/browser/ui/color/chrome_color_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"

void AddArgonColorMixer(ui::ColorProvider* provider,
                        const ui::ColorProviderKey& key) {
  if (key.custom_theme ||
      key.contrast_mode == ui::ColorProviderKey::ContrastMode::kHigh) {
    return;
  }
  const bool dark = key.color_mode == ui::ColorProviderKey::ColorMode::kDark;
  // Paper #f3f1ed on a white page; Dark #1c1b19 on #272623.
  const SkColor frame =
      dark ? SkColorSetRGB(0x1c, 0x1b, 0x19) : SkColorSetRGB(0xf3, 0xf1, 0xed);
  const SkColor page = dark ? SkColorSetRGB(0x27, 0x26, 0x23) : SK_ColorWHITE;

  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[ui::kColorFrameActive] = {frame};
  mixer[ui::kColorFrameInactive] = {frame};
  mixer[kColorToolbar] = {page};
}

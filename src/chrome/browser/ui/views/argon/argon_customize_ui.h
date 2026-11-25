// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_CUSTOMIZE_UI_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_CUSTOMIZE_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace argon {

// chrome://customize (helium://customize is rewritten to it), the Customize
// page (design_handoff_argon_browser/README.md §6). Kept out of
// chrome/common/webui_url_constants.h, which half the tree includes.
inline constexpr char kCustomizeHost[] = "customize";
inline constexpr char kCustomizeURL[] = "chrome://customize/";

class CustomizeUI;

class CustomizeUIConfig : public content::DefaultWebUIConfig<CustomizeUI> {
 public:
  CustomizeUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme, kCustomizeHost) {}
};

// Serves customize/ (compiled in, argon_customize_resources.h) and edits the
// prefs in argon_prefs.h, which the sidebar and frame redraw from.
class CustomizeUI : public content::WebUIController {
 public:
  explicit CustomizeUI(content::WebUI* web_ui);
  CustomizeUI(const CustomizeUI&) = delete;
  CustomizeUI& operator=(const CustomizeUI&) = delete;
  ~CustomizeUI() override;
};

}  // namespace argon

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_CUSTOMIZE_UI_H_

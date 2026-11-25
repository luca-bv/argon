// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_customize_ui.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/apple/bundle_locations.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/argon/argon_customize_resources.h"
#include "chrome/browser/ui/views/argon/argon_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace argon {

namespace {

// Allo, for the preview: the browser registers it for its own process only
// (argon_font.cc), so the page loads it as a web font.
constexpr std::string_view kFontPath = "allo.ttf";

bool ShouldHandle(const std::string& path) {
  return true;
}

// customize/ by name; anything else (the page itself is "") gets the page.
void Handle(const std::string& path,
            content::WebUIDataSource::GotDataCallback done) {
  if (path == kFontPath) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce([] {
          std::string font;
          base::ReadFileToString(base::apple::FrameworkBundlePath()
                                     .Append("Resources")
                                     .Append("AlloVariable.ttf"),
                                 &font);
          return font;
        }),
        base::BindOnce(
            [](content::WebUIDataSource::GotDataCallback done,
               std::string font) {
              std::move(done).Run(
                  base::MakeRefCounted<base::RefCountedString>(
                      std::move(font)));
            },
            std::move(done)));
    return;
  }
  base::span<const uint8_t> data = kCustomizeFiles.front().data;  // the page
  for (const CustomizeFile& file : kCustomizeFiles) {
    if (path == file.path) {
      data = file.data;
    }
  }
  std::move(done).Run(
      base::MakeRefCounted<base::RefCountedStaticMemory>(data));
}

// The page's whole state. tone and scheme are the profile's
// (prefs::kBrowserColorScheme: 0 system, 1 light, 2 dark).
base::DictValue State(Profile* profile) {
  const PrefService* local_state = g_browser_process->local_state();
  const PrefService* prefs = profile->GetPrefs();
  return base::DictValue()
      .Set("customize",
           local_state ? local_state->GetDict(kCustomizePref).Clone()
                       : base::DictValue())
      .Set("tone", prefs->GetString(kTonePref))
      .Set("scheme", prefs->GetInteger(prefs::kBrowserColorScheme))
      // Off the record: appearance is the original profile's, and writes
      // here would vanish with the profile.
      .Set("appearanceEditable", !profile->IsOffTheRecord());
}

class CustomizeHandler : public content::WebUIMessageHandler {
 public:
  explicit CustomizeHandler(Profile* profile) : profile_(profile) {}
  CustomizeHandler(const CustomizeHandler&) = delete;
  CustomizeHandler& operator=(const CustomizeHandler&) = delete;
  ~CustomizeHandler() override = default;

  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getState", base::BindRepeating(&CustomizeHandler::GetState,
                                        base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "setCustomize", base::BindRepeating(&CustomizeHandler::SetCustomize,
                                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "setAppearance", base::BindRepeating(&CustomizeHandler::SetAppearance,
                                             base::Unretained(this)));
  }

  void OnJavascriptAllowed() override {
    const auto push = base::BindRepeating(&CustomizeHandler::PushState,
                                          base::Unretained(this));
    if (PrefService* local_state = g_browser_process->local_state()) {
      local_state_registrar_.Init(local_state);
      local_state_registrar_.Add(kCustomizePref, push);
    }
    profile_registrar_.Init(profile_->GetPrefs());
    profile_registrar_.Add(kTonePref, push);
    profile_registrar_.Add(prefs::kBrowserColorScheme, push);
  }

  void OnJavascriptDisallowed() override {
    local_state_registrar_.Reset();
    profile_registrar_.Reset();
  }

 private:
  // [callback id]
  void GetState(const base::ListValue& args) {
    AllowJavascript();
    ResolveJavascriptCallback(args[0], State(profile_));
  }

  // [{zones, cfg}]
  void SetCustomize(const base::ListValue& args) {
    PrefService* local_state = g_browser_process->local_state();
    if (!local_state || args.size() != 1 || !args[0].is_dict()) {
      return;
    }
    local_state->SetDict(kCustomizePref, args[0].GetDict().Clone());
  }

  // [scheme 0–2, tone]
  void SetAppearance(const base::ListValue& args) {
    if (profile_->IsOffTheRecord() || args.size() != 2 || !args[0].is_int() ||
        !args[1].is_string()) {
      return;
    }
    const int scheme = args[0].GetInt();
    const std::string& tone = args[1].GetString();
    if (scheme < 0 || scheme > 2 ||
        (tone != "paper" && tone != "cool" && tone != "tinted")) {
      return;
    }
    PrefService* prefs = profile_->GetPrefs();
    prefs->SetString(kTonePref, tone);
    prefs->SetInteger(prefs::kBrowserColorScheme, scheme);
  }

  void PushState() {
    FireWebUIListener("argon-state-changed", State(profile_));
  }

  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar local_state_registrar_;
  PrefChangeRegistrar profile_registrar_;
};

}  // namespace

CustomizeUI::CustomizeUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, kCustomizeHost);
  source->SetRequestFilter(base::BindRepeating(&ShouldHandle),
                           base::BindRepeating(&Handle));
  web_ui->AddMessageHandler(std::make_unique<CustomizeHandler>(profile));
}

CustomizeUI::~CustomizeUI() = default;

}  // namespace argon

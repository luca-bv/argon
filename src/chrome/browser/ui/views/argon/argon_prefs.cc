// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_prefs.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace argon {

namespace {

constexpr auto kModules =
    base::MakeFixedFlatMap<std::string_view, ArgonModule>({
    {"nav", ArgonModule::kNavigation},
    {"spaceName", ArgonModule::kSpaceName},
    {"divider", ArgonModule::kDivider},
    {"favorites", ArgonModule::kPinned},
    {"pinned", ArgonModule::kSpacePins},
    {"loose", ArgonModule::kLoose},
    {"newTab", ArgonModule::kNewTab},
    {"spaces", ArgonModule::kSpaces},
});

// The "Quiet" layout as Argon draws it today.
std::vector<ArgonModule> DefaultSide() {
  return {ArgonModule::kNavigation, ArgonModule::kSpaceName,
          ArgonModule::kDivider,    ArgonModule::kPinned,
          ArgonModule::kDivider,    ArgonModule::kSpacePins,
          ArgonModule::kDivider,    ArgonModule::kLoose,
          ArgonModule::kNewTab,     ArgonModule::kDivider,
          ArgonModule::kSpaces};
}

// Known module ids in order; unknown ones (stage 3's address, clear, archive,
// spacer) are skipped, and only dividers may repeat.
std::vector<ArgonModule> ParseSide(const base::ListValue& list) {
  std::vector<ArgonModule> side;
  for (const base::Value& entry : list) {
    const std::string* id = entry.GetIfString();
    const auto it = id ? kModules.find(*id) : kModules.end();
    if (it == kModules.end()) {
      continue;
    }
    if (it->second != ArgonModule::kDivider &&
        std::ranges::contains(side, it->second)) {
      continue;
    }
    side.push_back(it->second);
  }
  return side;
}

// A string setting, or "" (which matches no choice) when missing.
std::string_view Str(const base::DictValue& cfg, std::string_view key) {
  const std::string* value = cfg.FindString(key);
  return value ? std::string_view(*value) : std::string_view();
}

// The frame color of a light tone, drawn after Argon's own mixer
// (argon_color_mixer.cc, which draws Paper and Dark).
class ToneMixer final : public ui::ColorProviderKey::InitializerSupplier {
 public:
  explicit ToneMixer(SkColor frame) : frame_(frame) {}
  ~ToneMixer() override = default;

  void AddColorMixers(ui::ColorProvider* provider,
                      const ui::ColorProviderKey& key) const override {
    if (key.custom_theme ||
        key.color_mode == ui::ColorProviderKey::ColorMode::kDark ||
        key.contrast_mode == ui::ColorProviderKey::ContrastMode::kHigh) {
      return;
    }
    ui::ColorMixer& mixer = provider->AddMixer();
    mixer[ui::kColorFrameActive] = {frame_};
    mixer[ui::kColorFrameInactive] = {frame_};
  }

 private:
  const SkColor frame_;
};

}  // namespace

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kCustomizePref);
}

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kTonePref, "paper");
}

Settings::Settings() : side(DefaultSide()) {}
Settings::Settings(const Settings&) = default;
Settings& Settings::operator=(const Settings&) = default;
Settings::~Settings() = default;

// static
Settings Settings::Load() {
  Settings settings;
  const PrefService* local_state =
      g_browser_process ? g_browser_process->local_state() : nullptr;
  if (!local_state) {
    return settings;
  }
  const base::DictValue& stored = local_state->GetDict(kCustomizePref);

  const base::DictValue* zones = stored.FindDict("zones");
  if (const base::ListValue* side = zones ? zones->FindList("side") : nullptr) {
    settings.side = ParseSide(*side);
  }

  const base::DictValue* cfg = stored.FindDict("cfg");
  if (!cfg) {
    return settings;
  }
  // Unknown values keep the default.
  const std::string_view density = Str(*cfg, "density");
  if (density == "Compact") {
    settings.density = Density::kCompact;
  } else if (density == "Roomy") {
    settings.density = Density::kRoomy;
  }
  const std::string_view icon = Str(*cfg, "icon");
  if (icon == "Dot") {
    settings.icon = TabIcon::kDot;
  } else if (icon == "None") {
    settings.icon = TabIcon::kNone;
  }
  settings.close_on_hover =
      cfg->FindBool("closeHover").value_or(settings.close_on_hover);
  const std::string_view archive = Str(*cfg, "archive");
  if (archive == "24h") {
    settings.archive_after = base::Hours(24);
  } else if (archive == "7d") {
    settings.archive_after = base::Days(7);
  } else if (archive == "Never") {
    settings.archive_after = std::nullopt;
  }
  settings.edge_pill = cfg->FindBool("edgePill").value_or(settings.edge_pill);
  settings.gap = std::clamp(cfg->FindInt("gap").value_or(settings.gap), 0, 16);
  settings.radius =
      std::clamp(cfg->FindInt("radius").value_or(settings.radius), 0, 20);
  return settings;
}

int PageGap() {
  return Settings::Load().gap;
}

int PageRadius() {
  return Settings::Load().radius;
}

void ApplyTone(const PrefService& profile_prefs, ui::ColorProviderKey& key) {
  static base::NoDestructor<ToneMixer> cool(SkColorSetRGB(0xec, 0xef, 0xf3));
  // Web-app windows keep their own controller (BrowserWidget sets it after);
  // a key that already carries a tone gets the current one.
  if (key.app_controller && key.app_controller.get() != cool.get()) {
    return;
  }
  // ponytail: Tinted is drawn as Paper until spaces get their own hues
  // (Customize stage 3).
  key.app_controller =
      profile_prefs.GetString(kTonePref) == "cool" ? cool.get() : nullptr;
}

}  // namespace argon

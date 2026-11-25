// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_arc_import.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/argon/argon_pins.h"
#include "chrome/browser/ui/views/argon/argon_spaces.h"
#include "chrome/browser/ui/views/argon/argon_toast.h"
#include "chrome/browser/ui/views/argon/argon_window_set.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "url/gurl.h"

namespace {

// Helium's per-profile prefs, as in argon_space_menu.cc.
constexpr char kHeliumDidOnboarding[] = "helium.completed_onboarding";
constexpr char kHeliumServicesConsented[] = "helium.services.user_consented";

struct ArcTab {
  GURL url;
  std::u16string title;
};

struct ArcSpace {
  std::u16string title;
  tab_groups::TabGroupColorId color = tab_groups::TabGroupColorId::kGrey;
  std::vector<ArcTab> pins;
  std::vector<ArcTab> tabs;
};

// One Arc profile's spaces and Favorites. Keyed by profile directory; empty
// for Arc's default profile.
struct ArcProfile {
  std::u16string name;
  std::vector<ArcSpace> spaces;
  std::vector<ArcTab> favorites;
};

using ArcData = std::map<std::string, ArcProfile>;

struct ArcItem {
  std::string title;
  std::string saved_url;
  std::string saved_title;
  bool is_tab = false;
  std::vector<std::string> children;
};

// The items list alternates id strings and item objects.
std::map<std::string, ArcItem> ParseItems(const base::ListValue& list) {
  std::map<std::string, ArcItem> items;
  for (size_t i = 0; i + 1 < list.size(); i += 2) {
    const std::string* id = list[i].GetIfString();
    const base::DictValue* dict = list[i + 1].GetIfDict();
    if (!id || !dict) {
      continue;
    }
    ArcItem item;
    if (const std::string* title = dict->FindString("title")) {
      item.title = *title;
    }
    if (const base::ListValue* children = dict->FindList("childrenIds")) {
      for (const base::Value& child : *children) {
        if (const std::string* child_id = child.GetIfString()) {
          item.children.push_back(*child_id);
        }
      }
    }
    const base::DictValue* data = dict->FindDict("data");
    if (const base::DictValue* tab = data ? data->FindDict("tab") : nullptr) {
      item.is_tab = true;
      if (const std::string* url = tab->FindString("savedURL")) {
        item.saved_url = *url;
      }
      if (const std::string* title = tab->FindString("savedTitle")) {
        item.saved_title = *title;
      }
    }
    items[*id] = std::move(item);
  }
  return items;
}

// The tabs under the container `id`, folders flattened, in order. Only web
// pages; `visited` guards against cycles.
void CollectTabs(const std::map<std::string, ArcItem>& items,
                 const std::string& id,
                 std::set<std::string>& visited,
                 std::vector<ArcTab>& out) {
  const auto it = items.find(id);
  if (it == items.end() || !visited.insert(id).second) {
    return;
  }
  const ArcItem& item = it->second;
  if (item.is_tab) {
    const GURL url(item.saved_url);
    if (url.SchemeIsHTTPOrHTTPS()) {
      out.push_back({url, base::UTF8ToUTF16(item.title.empty() ? item.saved_title
                                                                : item.title)});
    }
  }
  for (const std::string& child : item.children) {
    CollectTabs(items, child, visited, out);
  }
}

// The profile directory of an Arc profile object: empty for {default: true},
// else custom._0.directoryBasename.
std::string ProfileDirectory(const base::DictValue& profile) {
  const std::string* dir =
      profile.FindStringByDottedPath("custom._0.directoryBasename");
  return dir ? *dir : std::string();
}

// The nearest of Chromium's nine group colors to an Arc theme color.
tab_groups::TabGroupColorId NearestColor(double r, double g, double b) {
  const double max = std::max({r, g, b});
  const double min = std::min({r, g, b});
  if (max - min < 0.12) {
    return tab_groups::TabGroupColorId::kGrey;
  }
  double hue = 0;
  if (max == r) {
    hue = std::fmod((g - b) / (max - min), 6.0);
  } else if (max == g) {
    hue = (b - r) / (max - min) + 2;
  } else {
    hue = (r - g) / (max - min) + 4;
  }
  hue = std::fmod(hue * 60 + 360, 360);
  static constexpr std::pair<double, tab_groups::TabGroupColorId> kHues[] = {
      {0, tab_groups::TabGroupColorId::kRed},
      {30, tab_groups::TabGroupColorId::kOrange},
      {52, tab_groups::TabGroupColorId::kYellow},
      {125, tab_groups::TabGroupColorId::kGreen},
      {185, tab_groups::TabGroupColorId::kCyan},
      {220, tab_groups::TabGroupColorId::kBlue},
      {270, tab_groups::TabGroupColorId::kPurple},
      {325, tab_groups::TabGroupColorId::kPink},
      {360, tab_groups::TabGroupColorId::kRed},
  };
  tab_groups::TabGroupColorId nearest = tab_groups::TabGroupColorId::kGrey;
  double best = 1000;
  for (const auto& [h, color] : kHues) {
    if (std::abs(hue - h) < best) {
      best = std::abs(hue - h);
      nearest = color;
    }
  }
  return nearest;
}

// Runs on the thread pool. Empty if Arc's data isn't there.
ArcData ReadArcData() {
  ArcData data;
  base::FilePath app_data;
  if (!base::PathService::Get(base::DIR_APP_DATA, &app_data)) {
    return data;
  }
  const base::FilePath arc = app_data.Append("Arc");
  std::string content;
  if (!base::ReadFileToString(arc.Append("StorableSidebar.json"), &content)) {
    return data;
  }
  std::optional<base::DictValue> root =
      base::JSONReader::ReadDict(content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  const base::DictValue* sidebar = root ? root->FindDict("sidebar") : nullptr;
  const base::ListValue* containers =
      sidebar ? sidebar->FindList("containers") : nullptr;
  if (!containers) {
    return data;
  }
  // The container that holds the spaces.
  const base::DictValue* container = nullptr;
  for (const base::Value& value : *containers) {
    if (value.is_dict() && value.GetDict().FindList("spaces")) {
      container = &value.GetDict();
      break;
    }
  }
  const base::ListValue* item_list =
      container ? container->FindList("items") : nullptr;
  if (!item_list) {
    return data;
  }
  const std::map<std::string, ArcItem> items = ParseItems(*item_list);

  // Spaces alternate id strings and space objects.
  const base::ListValue& spaces = *container->FindList("spaces");
  for (size_t i = 0; i + 1 < spaces.size(); i += 2) {
    const base::DictValue* dict = spaces[i + 1].GetIfDict();
    if (!dict) {
      continue;
    }
    ArcSpace space;
    if (const std::string* title = dict->FindString("title")) {
      space.title = base::UTF8ToUTF16(*title);
    }
    if (const base::DictValue* tone = dict->FindDictByDottedPath(
            "customInfo.windowTheme.primaryColorPalette.midTone")) {
      space.color = NearestColor(tone->FindDouble("red").value_or(0),
                                 tone->FindDouble("green").value_or(0),
                                 tone->FindDouble("blue").value_or(0));
    }
    // containerIDs: ["pinned", <id>, "unpinned", <id>].
    if (const base::ListValue* ids = dict->FindList("containerIDs")) {
      for (size_t j = 0; j + 1 < ids->size(); j += 2) {
        const std::string* label = (*ids)[j].GetIfString();
        const std::string* id = (*ids)[j + 1].GetIfString();
        if (!label || !id) {
          continue;
        }
        const auto it = items.find(*id);
        if (it == items.end()) {
          continue;
        }
        std::set<std::string> visited = {*id};
        std::vector<ArcTab>& out =
            *label == "pinned" ? space.pins : space.tabs;
        for (const std::string& child : it->second.children) {
          CollectTabs(items, child, visited, out);
        }
      }
    }
    const base::DictValue* profile = dict->FindDict("profile");
    const std::string dir = profile ? ProfileDirectory(*profile) : "";
    data[dir].spaces.push_back(std::move(space));
  }

  // Favorites: profile objects alternating with container ids.
  if (const base::ListValue* top = container->FindList("topAppsContainerIDs")) {
    for (size_t i = 0; i + 1 < top->size(); i += 2) {
      const base::DictValue* profile = (*top)[i].GetIfDict();
      const std::string* id = (*top)[i + 1].GetIfString();
      if (!profile || !id) {
        continue;
      }
      const auto it = items.find(*id);
      if (it == items.end()) {
        continue;
      }
      std::set<std::string> visited = {*id};
      ArcProfile& arc_profile = data[ProfileDirectory(*profile)];
      for (const std::string& child : it->second.children) {
        CollectTabs(items, child, visited, arc_profile.favorites);
      }
    }
  }

  // Profile names from Arc's own Chromium Local State.
  std::string local_state;
  if (base::ReadFileToString(arc.Append("User Data").Append("Local State"),
                             &local_state)) {
    if (std::optional<base::DictValue> state =
            base::JSONReader::ReadDict(local_state,
                                       base::JSON_PARSE_CHROMIUM_EXTENSIONS)) {
      for (auto& [dir, profile] : data) {
        if (dir.empty()) {
          continue;
        }
        if (const base::DictValue* cache =
                state->FindDictByDottedPath("profile.info_cache")) {
          if (const base::DictValue* info = cache->FindDict(dir)) {
            if (const std::string* name = info->FindString("name")) {
              profile.name = base::UTF8ToUTF16(*name);
            }
          }
        }
      }
    }
  }
  for (auto& [dir, profile] : data) {
    if (!dir.empty() && profile.name.empty()) {
      profile.name = base::UTF8ToUTF16(dir);
    }
  }
  return data;
}

// Whether an Arc profile has anything to import (Arc lists every profile's
// Favorites container, empty or not).
bool HasContent(const ArcProfile& profile) {
  if (!profile.favorites.empty()) {
    return true;
  }
  for (const ArcSpace& space : profile.spaces) {
    if (!space.pins.empty() || !space.tabs.empty()) {
      return true;
    }
  }
  return false;
}

// Adds one Arc profile's Favorites and spaces to `target`. Returns the spaces
// imported.
int AddToWindow(SessionID importing,
                ArcProfile profile,
                BrowserWindowInterface* target) {
  if (!target) {
    return 0;
  }
  TabStripModel* model = target->GetTabStripModel();

  // Favorites, unless already a global pin.
  std::set<GURL> pinned;
  for (int i = 0; i < model->count() && model->IsTabPinned(i); ++i) {
    pinned.insert(model->GetTabAtIndex(i)->GetContents()->GetVisibleURL());
  }
  for (const ArcTab& tab : profile.favorites) {
    if (pinned.insert(tab.url).second) {
      chrome::AddDeferredTab(target, tab.url, tab.title,
                             model->IndexOfFirstNonPinnedTab(), /*pin=*/true);
    }
  }

  // Spaces, unless one of that name exists.
  std::set<std::u16string> names;
  if (model->SupportsTabGroups()) {
    for (const tab_groups::TabGroupId& id :
         model->group_model()->ListTabGroups()) {
      names.insert(model->group_model()->GetTabGroup(id)->visual_data()->title());
    }
  }
  int imported = 0;
  for (const ArcSpace& space : profile.spaces) {
    if (space.pins.empty() && space.tabs.empty()) {
      continue;
    }
    std::u16string title = space.title.empty() ? u"Space" : space.title;
    if (!names.insert(title).second) {
      continue;
    }
    std::vector<content::WebContents*> added;
    for (const ArcTab& tab : space.pins) {
      if (content::WebContents* contents = chrome::AddDeferredTab(
              target, tab.url, tab.title, model->count(), /*pin=*/false)) {
        argon_pins::SetPin(contents, tab.url);
        added.push_back(contents);
      }
    }
    for (const ArcTab& tab : space.tabs) {
      if (content::WebContents* contents = chrome::AddDeferredTab(
              target, tab.url, tab.title, model->count(), /*pin=*/false)) {
        added.push_back(contents);
      }
    }
    std::vector<int> indices;
    for (content::WebContents* contents : added) {
      const int index = model->GetIndexOfWebContents(contents);
      if (index != TabStripModel::kNoTab) {
        indices.push_back(index);
      }
    }
    if (indices.empty()) {
      continue;
    }
    std::sort(indices.begin(), indices.end());
    const tab_groups::TabGroupId group = model->AddToNewGroup(indices);
    model->ChangeTabGroupVisuals(
        group, tab_groups::TabGroupVisualData(std::move(title), space.color));
    ++imported;
  }
  if (ArgonSpaces* spaces = ArgonWindowSet::Get()->SpacesFor(target)) {
    spaces->PinsChanged();
  }
  return imported;
}

// For a profile whose window the import just opened, which came forward: the
// importing window comes back and the new one is hidden behind it, now rather
// than by the set's posted activation rule, whose order isn't fixed.
void AddToWindowAsync(SessionID importing,
                      ArcProfile profile,
                      BrowserWindowInterface* target) {
  AddToWindow(importing, std::move(profile), target);
  BrowserWindowInterface* back = BrowserWindowInterface::FromSessionID(importing);
  if (!target || !back || back == target || back->GetWindow()->IsFullscreen()) {
    return;
  }
  back->GetWindow()->Show();
  back->GetWindow()->Activate();
  ArgonWindowSet::Get()->Hide(target);
}

// Opens (or finds) the window of the profile at `path` and adds `profile`.
void AddToProfile(SessionID importing,
                  ArcProfile profile,
                  const base::FilePath& path) {
  if (BrowserWindowInterface* member = ArgonWindowSet::Get()->MemberFor(path)) {
    AddToWindow(importing, std::move(profile), member);
    return;
  }
  profiles::SwitchToProfile(
      path, /*always_create=*/false,
      base::BindOnce(&AddToWindowAsync, importing, std::move(profile)));
}

void Apply(SessionID browser_id, ArcData data) {
  BrowserWindowInterface* browser =
      BrowserWindowInterface::FromSessionID(browser_id);
  if (!browser) {
    return;
  }
  std::erase_if(data, [](const auto& entry) { return !HasContent(entry.second); });
  if (data.empty()) {
    ShowArgonToast(browser, u"No Arc sidebar found on this Mac",
                   base::OnceClosure());
    return;
  }
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  const bool consented =
      browser->GetProfile()->GetPrefs()->GetBoolean(kHeliumServicesConsented);
  int imported = 0;
  int other_profiles = 0;
  for (auto& [dir, profile] : data) {
    // Arc's default profile is the one importing.
    if (dir.empty()) {
      imported = AddToWindow(browser_id, std::move(profile), browser);
      continue;
    }
    ++other_profiles;
    // Another Arc profile: the Argon profile of the same name, or a new one.
    std::optional<base::FilePath> path;
    for (ProfileAttributesEntry* entry : storage.GetAllProfilesAttributes()) {
      if (!entry->IsOmitted() && entry->GetLocalProfileName() == profile.name) {
        path = entry->GetPath();
        break;
      }
    }
    if (path) {
      AddToProfile(browser_id, std::move(profile), *path);
      continue;
    }
    const std::u16string name = profile.name;
    ProfileManager::CreateMultiProfileAsync(
        name, storage.ChooseAvatarIconIndexForNewProfile(),
        /*is_hidden=*/false,
        base::BindOnce(
            [](SessionID importing, ArcProfile profile, bool consented,
               Profile* created) {
              if (!created) {
                return;
              }
              created->GetPrefs()->SetBoolean(kHeliumDidOnboarding, true);
              created->GetPrefs()->SetBoolean(kHeliumServicesConsented,
                                              consented);
              AddToProfile(importing, std::move(profile), created->GetPath());
            },
            browser_id, std::move(profile), consented));
  }
  // One toast, here; other profiles' spaces arrive in their own windows.
  std::u16string text = u"Imported " + base::NumberToString16(imported) +
                        (imported == 1 ? u" space" : u" spaces") +
                        u" from Arc";
  if (other_profiles) {
    text += u", and " + base::NumberToString16(other_profiles) +
            (other_profiles == 1 ? u" profile" : u" profiles");
  }
  ShowArgonToast(browser, std::move(text), base::OnceClosure());
}

}  // namespace

void ImportFromArc(BrowserWindowInterface* browser) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadArcData),
      base::BindOnce(&Apply, browser->GetSessionID()));
}

// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_space_menu.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/argon/argon_pins.h"
#include "chrome/browser/ui/views/argon/argon_toast.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/core/session_id.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// Helium's per-profile prefs (components/helium_services/pref_names.h), by key
// to avoid a dependency on Helium's component for two strings.
constexpr char kHeliumDidOnboarding[] = "helium.completed_onboarding";
constexpr char kHeliumServicesConsented[] = "helium.services.user_consented";

constexpr int kNewProfileCommand = 0;
constexpr int kColorMenuCommand = 1;
// Commands from here up pick the profile at (command - kFirstProfileCommand).
constexpr int kFirstProfileCommand = 2;
// Commands from here up pick the color kFirstColorCommand + TabGroupColorId.
constexpr int kFirstColorCommand = 1000;

// What a moved space keeps: its name, color, tabs' addresses and which of
// them are pins (with their pinned URLs; empty for loose tabs).
struct SpaceContents {
  tab_groups::TabGroupVisualData visuals;
  std::vector<GURL> urls;
  std::vector<GURL> pins;
};

void MoveSpace(SessionID from_id,
               tab_groups::TabGroupId space,
               base::FilePath to_profile);

// Reopens `contents` as a new space in `target`, shows it in place of the
// window `from_id`, and closes the space's original tabs there.
void OpenSpaceIn(SpaceContents contents,
                 SessionID from_id,
                 tab_groups::TabGroupId old_space,
                 BrowserWindowInterface* target) {
  BrowserWindowInterface* from = BrowserWindowInterface::FromSessionID(from_id);
  if (!target || !from || target == from || contents.urls.empty()) {
    return;
  }
  TabStripModel* to_model = target->GetTabStripModel();
  std::vector<content::WebContents*> added;
  for (size_t i = 0; i < contents.urls.size(); ++i) {
    if (content::WebContents* web_contents = chrome::AddAndReturnTabAt(
            target, contents.urls[i], /*index=*/-1, /*foreground=*/false)) {
      if (contents.pins[i].is_valid()) {
        argon_pins::SetPin(web_contents, contents.pins[i]);
      }
      added.push_back(web_contents);
    }
  }
  std::vector<int> indices;
  for (content::WebContents* web_contents : added) {
    const int index = to_model->GetIndexOfWebContents(web_contents);
    if (index != TabStripModel::kNoTab) {
      indices.push_back(index);
    }
  }
  if (indices.empty()) {
    return;
  }
  std::sort(indices.begin(), indices.end());
  const tab_groups::TabGroupId new_space = to_model->AddToNewGroup(indices);
  to_model->ChangeTabGroupVisuals(new_space, contents.visuals);
  // Shows `target` at `from`'s frame (adding tabs doesn't show a hidden
  // window), before the toast is placed on it.
  ArgonWindowSet::Get()->Switch(from, {target, new_space});

  // Close the originals. A window whose last tab closes closes too, so then a
  // new tab is opened first, in a new unnamed space ("Home"), not the old one.
  TabStripModel* from_model = from->GetTabStripModel();
  if (from_model->group_model()->ContainsTabGroup(old_space)) {
    const gfx::Range range =
        from_model->group_model()->GetTabGroup(old_space)->ListTabs();
    std::vector<base::WeakPtr<tabs::TabInterface>> originals;
    for (int i = static_cast<int>(range.start());
         i < static_cast<int>(range.end()); ++i) {
      originals.push_back(from_model->GetTabAtIndex(i)->GetWeakPtr());
    }
    // (AddAndReturnTabAt, not chrome::NewTab, which would show the window
    // Switch() just hid.)
    if (static_cast<int>(originals.size()) == from_model->count()) {
      if (content::WebContents* blank = chrome::AddAndReturnTabAt(
              from, GURL(), /*index=*/-1, /*foreground=*/true)) {
        from_model->AddToNewGroup(
            {from_model->GetIndexOfWebContents(blank)});
      }
    }
    for (const auto& tab : originals) {
      const int index = tab ? from_model->GetIndexOfTab(tab.get())
                            : TabStripModel::kNoTab;
      if (index != TabStripModel::kNoTab) {
        from_model->CloseWebContentsAt(index, TabCloseTypes::CLOSE_NONE);
      }
    }
  }

  ShowArgonToast(target, u"Moved \u201c" + contents.visuals.title() +
                             u"\u201d to this profile",
                 base::BindOnce(&MoveSpace, target->GetSessionID(), new_space,
                                from->GetProfile()->GetPath()));
}

// Moves `space` of the window `from_id` into `to_profile`'s window.
void MoveSpace(SessionID from_id,
               tab_groups::TabGroupId space,
               base::FilePath to_profile) {
  BrowserWindowInterface* from = BrowserWindowInterface::FromSessionID(from_id);
  if (!from || from->GetProfile()->GetPath() == to_profile) {
    return;
  }
  TabStripModel* model = from->GetTabStripModel();
  // The space may be gone by now (Undo after it was closed or archived).
  if (!model->group_model()->ContainsTabGroup(space)) {
    return;
  }
  const TabGroup* group = model->group_model()->GetTabGroup(space);
  SpaceContents contents{*group->visual_data(), {}, {}};
  const gfx::Range range = group->ListTabs();
  for (int i = static_cast<int>(range.start());
       i < static_cast<int>(range.end()); ++i) {
    content::WebContents* web_contents = model->GetTabAtIndex(i)->GetContents();
    // A page that would ask "Leave site?" keeps the space where it is.
    if (web_contents->NeedToFireBeforeUnload()) {
      ShowArgonToast(from, u"A page in this space has unsaved changes",
                     base::OnceClosure());
      return;
    }
    const GURL& url = web_contents->GetLastCommittedURL();
    contents.urls.push_back(url.is_empty() ? web_contents->GetVisibleURL()
                                           : url);
    contents.pins.push_back(argon_pins::PinnedUrl(web_contents));
  }
  // The profile's window in the set (it may be hidden); open one only when it
  // has none.
  if (BrowserWindowInterface* target =
          ArgonWindowSet::Get()->MemberFor(to_profile)) {
    OpenSpaceIn(std::move(contents), from_id, space, target);
    return;
  }
  profiles::SwitchToProfile(
      to_profile, /*always_create=*/false,
      base::BindOnce(&OpenSpaceIn, std::move(contents), from_id, space));
}

class SpaceMenu : public ui::SimpleMenuModel::Delegate {
 public:
  explicit SpaceMenu(const ArgonWindowSet::Space& space)
      : from_id_(space.browser->GetSessionID()),
        space_(space.id),
        current_profile_(space.browser->GetProfile()->GetPath()) {
    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    for (ProfileAttributesEntry* entry :
         storage.GetAllProfilesAttributesSortedByLocalProfileNameWithCheck()) {
      if (entry->IsOmitted()) {
        continue;
      }
      model_.AddCheckItem(
          kFirstProfileCommand + static_cast<int>(profiles_.size()),
          entry->GetLocalProfileName());
      profiles_.push_back(entry->GetPath());
    }
    model_.AddSeparator(ui::NORMAL_SEPARATOR);
    model_.AddItem(kNewProfileCommand, u"New profile for this space");
    // The space's accent (its tab group's color).
    for (const auto& [color, label] : tab_groups::GetTabGroupColorLabelMap()) {
      colors_model_.AddRadioItem(kFirstColorCommand + static_cast<int>(color),
                                 label, /*group_id=*/0);
    }
    model_.AddSeparator(ui::NORMAL_SEPARATOR);
    model_.AddSubMenu(kColorMenuCommand, u"Color", &colors_model_);
  }
  SpaceMenu(const SpaceMenu&) = delete;
  SpaceMenu& operator=(const SpaceMenu&) = delete;
  ~SpaceMenu() override = default;

  void Run(views::Widget* parent,
           const gfx::Point& point,
           ui::mojom::MenuSourceType source_type) {
    runner_ = std::make_unique<views::MenuRunner>(
        &model_, views::MenuRunner::CONTEXT_MENU);
    runner_->RunMenuAt(parent, nullptr, gfx::Rect(point, gfx::Size()),
                       views::MenuAnchorPosition::kTopLeft, source_type);
  }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override {
    if (command_id >= kFirstColorCommand) {
      const TabGroup* group = Group();
      return group && static_cast<int>(group->visual_data()->color()) ==
                          command_id - kFirstColorCommand;
    }
    const size_t index = command_id - kFirstProfileCommand;
    return command_id >= kFirstProfileCommand && index < profiles_.size() &&
           profiles_[index] == current_profile_;
  }

  void ExecuteCommand(int command_id, int event_flags) override {
    if (command_id >= kFirstColorCommand) {
      if (const TabGroup* group = Group()) {
        BrowserWindowInterface::FromSessionID(from_id_)
            ->GetTabStripModel()
            ->ChangeTabGroupVisuals(
                space_, tab_groups::TabGroupVisualData(
                            group->visual_data()->title(),
                            static_cast<tab_groups::TabGroupColorId>(
                                command_id - kFirstColorCommand)));
      }
      return;
    }
    if (command_id == kNewProfileCommand) {
      NewProfile();
      return;
    }
    const size_t index = command_id - kFirstProfileCommand;
    if (index < profiles_.size()) {
      MoveSpace(from_id_, space_, profiles_[index]);
    }
  }

 private:
  // The space's group, or null if it (or its window) is gone.
  const TabGroup* Group() const {
    BrowserWindowInterface* from =
        BrowserWindowInterface::FromSessionID(from_id_);
    if (!from) {
      return nullptr;
    }
    const TabGroupModel* groups = from->GetTabStripModel()->group_model();
    return groups->ContainsTabGroup(space_) ? groups->GetTabGroup(space_)
                                            : nullptr;
  }

  void NewProfile() {
    BrowserWindowInterface* from = BrowserWindowInterface::FromSessionID(from_id_);
    if (!from) {
      return;
    }
    const TabGroupModel* groups = from->GetTabStripModel()->group_model();
    if (!groups->ContainsTabGroup(space_)) {
      return;
    }
    const std::u16string& title =
        groups->GetTabGroup(space_)->visual_data()->title();
    const std::u16string name = title.empty() ? u"Space" : title;
    // The new profile skips Helium's setup page and keeps the services consent
    // given in this one.
    const bool consented =
        from->GetProfile()->GetPrefs()->GetBoolean(kHeliumServicesConsented);
    ProfileAttributesStorage& storage =
        g_browser_process->profile_manager()->GetProfileAttributesStorage();
    ProfileManager::CreateMultiProfileAsync(
        name, storage.ChooseAvatarIconIndexForNewProfile(),
        /*is_hidden=*/false,
        base::BindOnce(
            [](SessionID from_id, tab_groups::TabGroupId space, bool consented,
               Profile* profile) {
              if (!profile) {
                return;
              }
              profile->GetPrefs()->SetBoolean(kHeliumDidOnboarding, true);
              profile->GetPrefs()->SetBoolean(kHeliumServicesConsented,
                                              consented);
              MoveSpace(from_id, space, profile->GetPath());
            },
            from_id_, space_, consented));
  }

  const SessionID from_id_;
  const tab_groups::TabGroupId space_;
  const base::FilePath current_profile_;
  std::vector<base::FilePath> profiles_;
  // Declared first: model_ points at it.
  ui::SimpleMenuModel colors_model_{this};
  ui::SimpleMenuModel model_{this};
  std::unique_ptr<views::MenuRunner> runner_;
};

// ponytail: the last menu is kept until the next one opens, since the runner
// must outlive the menu it shows.
std::unique_ptr<SpaceMenu>& LastMenu() {
  static base::NoDestructor<std::unique_ptr<SpaceMenu>> menu;
  return *menu;
}

}  // namespace

void ShowArgonSpaceMenu(views::View* source,
                        const ArgonWindowSet::Space& space,
                        const gfx::Point& point,
                        ui::mojom::MenuSourceType source_type) {
  if (!space.browser || !source->GetWidget()) {
    return;
  }
  LastMenu() = std::make_unique<SpaceMenu>(space);
  LastMenu()->Run(source->GetWidget(), point, source_type);
}

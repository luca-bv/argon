// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_spaces.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/argon/argon_pins.h"
#include "chrome/browser/ui/views/argon/argon_window_set.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace {

// ponytail: fixed names until spaces can be renamed from Argon's own UI; the
// tab group editor (right-click a group) renames them meanwhile.
constexpr char16_t kFirstSpaceName[] = u"Home";

// After launch, every tab gets at least this long, so a session restored with
// old tabs isn't archived a minute in.
constexpr base::TimeDelta kLaunchGrace = base::Hours(1);

base::Time LaunchTime() {
  static const base::Time launched = base::Time::Now();
  return launched;
}

// When each tab was last left. WebContents only records when it was last shown,
// so a tab used for hours would otherwise count as idle since it was opened.
// ponytail: keyed by pointer across windows, erased when a tab closes in a
// window with spaces; a stale entry only makes a tab last longer.
std::map<const tabs::TabInterface*, base::Time>& LeftAt() {
  static base::NoDestructor<std::map<const tabs::TabInterface*, base::Time>>
      left_at;
  return *left_at;
}

bool Enabled(const TabStripModel* model) {
  return model->SupportsTabGroups() &&
         base::FeatureList::IsEnabled(features::kTabGroupsFocusing);
}

}  // namespace

ArgonSpaces::ArgonSpaces(BrowserWindowInterface* browser)
    : browser_(browser), model_(browser->GetTabStripModel()) {
  model_->AddObserver(this);
  ArgonWindowSet::Get()->Register(browser_, this);
  LaunchTime();
  ScheduleEnforce();
  archive_timer_.Start(FROM_HERE, base::Minutes(1), this,
                       &ArgonSpaces::ArchiveIdleTabs);
}

// static
std::optional<base::TimeDelta> ArgonSpaces::TimeUntilArchive(
    const tabs::TabInterface* tab) {
  content::WebContents* contents = tab->GetContents();
  if (tab->IsPinned() || tab->IsActivated() || !contents ||
      argon_pins::IsPin(contents) ||
      contents->GetVisibility() == content::Visibility::VISIBLE ||
      contents->IsCurrentlyAudible() || contents->IsBeingCaptured() ||
      contents->NeedToFireBeforeUnload()) {
    return std::nullopt;
  }
  if (const std::optional<tab_groups::TabGroupId> space = tab->GetGroup()) {
    const BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
    const TabGroup* group =
        browser ? browser->GetTabStripModel()->group_model()->GetTabGroup(*space)
                : nullptr;
    if (!group || group->tab_count() <= 1) {
      return std::nullopt;
    }
  }
  base::Time idle_since = contents->GetLastActiveTime();
  if (const auto it = LeftAt().find(tab); it != LeftAt().end()) {
    idle_since = std::max(idle_since, it->second);
  }
  const base::Time deadline =
      std::max(idle_since + kArchiveAfter, LaunchTime() + kLaunchGrace);
  return deadline - base::Time::Now();
}

ArgonSpaces::~ArgonSpaces() {
  ArgonWindowSet::Get()->Unregister(browser_);
  model_->RemoveObserver(this);
}

std::vector<tab_groups::TabGroupId> ArgonSpaces::List() const {
  return ListIn(model_);
}

// static
std::vector<tab_groups::TabGroupId> ArgonSpaces::ListIn(
    const TabStripModel* model) {
  std::vector<tab_groups::TabGroupId> spaces;
  if (!Enabled(model)) {
    return spaces;
  }
  for (int i = 0; i < model->count(); ++i) {
    const std::optional<tab_groups::TabGroupId> group =
        model->GetTabAtIndex(i)->GetGroup();
    if (group && (spaces.empty() || spaces.back() != *group)) {
      spaces.push_back(*group);  // A group's tabs are contiguous.
    }
  }
  return spaces;
}

void ArgonSpaces::Switch(const tab_groups::TabGroupId& space) {
  if (!Enabled(model_) || !model_->group_model()->ContainsTabGroup(space)) {
    return;
  }
  // Remember the tab being left: one activated before it was grouped (a new
  // tab) was never recorded.
  if (tabs::TabInterface* leaving = model_->GetActiveTab()) {
    if (const std::optional<tab_groups::TabGroupId> group =
            leaving->GetGroup()) {
      last_active_[*group] = leaving->GetWeakPtr();
    }
  }
  // Entering focus mode activates the space's first tab unless the active tab
  // is already there (and records that as the space's last tab), so read the
  // one last used there first and go back to it after.
  base::WeakPtr<tabs::TabInterface> target;
  if (const auto it = last_active_.find(space); it != last_active_.end()) {
    target = it->second;
  }
  model_->EnterFocusMode(space);
  if (target && target->GetGroup() == space) {
    const int index = model_->GetIndexOfTab(target.get());
    if (index != TabStripModel::kNoTab) {
      model_->ActivateTabAt(index);
    }
  }
}

void ArgonSpaces::Create() {
  if (!Enabled(model_)) {
    return;
  }
  // The new tab may land in the current space (or nowhere, if that exited
  // focus mode); either way it moves into a space of its own.
  const tabs::TabInterface* before = model_->GetActiveTab();
  // Not IDC_NEW_TAB, which opens the command bar.
  chrome::NewTab(browser_, NewTabTypes::kNewTabCommand);
  const int index = model_->active_index();
  // No new tab (the command was blocked): don't move the current one.
  if (index == TabStripModel::kNoTab || model_->GetActiveTab() == before ||
      model_->IsTabPinned(index)) {
    return;
  }
  const tab_groups::TabGroupId space = NewSpace(
      {index}, u"Space " + base::NumberToString16(List().size() + 1));
  model_->EnterFocusMode(space);
}

void ArgonSpaces::SetClearCallback(base::RepeatingClosure clear) {
  clear_ = std::move(clear);
}

void ArgonSpaces::Clear() {
  if (clear_) {
    clear_.Run();
  }
}

bool ArgonSpaces::TogglePinOfActiveTab() {
  tabs::TabInterface* active = model_->GetActiveTab();
  if (!active || active->IsPinned() || !active->GetGroup()) {
    return false;
  }
  content::WebContents* contents = active->GetContents();
  const GURL& committed = contents->GetLastCommittedURL();
  argon_pins::SetPin(contents,
                     argon_pins::IsPin(contents)
                         ? GURL()
                         : (committed.is_empty() ? contents->GetVisibleURL()
                                                 : committed));
  PinsChanged();
  return true;
}

void ArgonSpaces::PinsChanged() {
  ScheduleEnforce();
  ArgonWindowSet::Get()->NotifyChanged(nullptr);
}

void ArgonSpaces::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() && selection.old_tab) {
    LeftAt()[selection.old_tab] = base::Time::Now();
  }
  // Discarding swaps a tab's WebContents; the pin comes along.
  if (change.type() == TabStripModelChange::kReplaced) {
    const TabStripModelChange::Replace* replace = change.GetReplace();
    if (argon_pins::IsPin(replace->old_contents)) {
      argon_pins::SetPin(replace->new_contents,
                         argon_pins::PinnedUrl(replace->old_contents));
    }
  }
  if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& removed : change.GetRemove()->contents) {
      LeftAt().erase(removed.tab);
    }
  }
  if (selection.active_tab_changed() && selection.new_tab) {
    if (const std::optional<tab_groups::TabGroupId> group =
            selection.new_tab->GetGroup()) {
      last_active_[*group] = selection.new_tab->GetWeakPtr();
    }
  }
  ScheduleEnforce();
}

void ArgonSpaces::OnTabPinnedStateChanged(tabs::TabInterface* tab,
                                          int index) {
  // A tab made a global pin (Favorite) stops being its space's pin.
  if (tab->IsPinned() && argon_pins::IsPin(tab->GetContents())) {
    argon_pins::SetPin(tab->GetContents(), GURL());
  }
  ScheduleEnforce();
}

void ArgonSpaces::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  ScheduleEnforce();
}

void ArgonSpaces::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  if (old_focused_group_id) {
    last_space_ = old_focused_group_id;
  }
  ScheduleEnforce();
}

void ArgonSpaces::ScheduleEnforce() {
  if (enforce_pending_) {
    return;
  }
  enforce_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ArgonSpaces::Enforce, weak_factory_.GetWeakPtr()));
}

void ArgonSpaces::Enforce() {
  enforce_pending_ = false;
  // Sidebars of the other windows in the set list this window's spaces too.
  ArgonWindowSet::Get()->NotifyChanged(browser_);
  if (!Enabled(model_) || model_->empty() || model_->closing_all()) {
    return;
  }
  const TabGroupModel* groups = model_->group_model();

  // The space that should be focused: the focused one; else the active tab's
  // (activating a tab in another space exits focus mode); else the last one;
  // else the first.
  std::optional<tab_groups::TabGroupId> space = model_->GetFocusedGroup();
  if (!space) {
    if (const tabs::TabInterface* active = model_->GetActiveTab()) {
      space = active->GetGroup();
    }
  }
  if (!space && last_space_ && groups->ContainsTabGroup(*last_space_)) {
    space = last_space_;
  }
  if (!space) {
    const std::vector<tab_groups::TabGroupId> spaces = List();
    if (!spaces.empty()) {
      space = spaces.front();
    }
  }

  // Unpinned tabs outside every space (new tabs, ungrouped tabs, tabs from
  // before Argon) join that space, or a new "Home".
  std::vector<int> loose;
  for (int i = 0; i < model_->count(); ++i) {
    const tabs::TabInterface* tab = model_->GetTabAtIndex(i);
    if (!tab->IsPinned() && !tab->GetGroup()) {
      loose.push_back(i);
    }
  }
  if (!loose.empty()) {
    if (space) {
      model_->AddToExistingGroup(loose, *space);
    } else {
      space = NewSpace(loose, kFirstSpaceName);
    }
  }

  // Each space's pins come first, in their order, so tab order matches the
  // sidebar.
  for (const tab_groups::TabGroupId& group_id : List()) {
    const gfx::Range range = groups->GetTabGroup(group_id)->ListTabs();
    int next_pin = static_cast<int>(range.start());
    for (int i = next_pin; i < static_cast<int>(range.end()); ++i) {
      if (!argon_pins::IsPin(model_->GetTabAtIndex(i)->GetContents())) {
        continue;
      }
      if (i != next_pin) {
        model_->MoveWebContentsAt(i, next_pin, /*select_after_move=*/false,
                                  group_id);
      }
      ++next_pin;
    }
  }

  // Only pinned tabs left: nothing to focus until a space exists again.
  if (space && groups->ContainsTabGroup(*space) &&
      model_->GetFocusedGroup() != space) {
    // Entering focus mode activates the space's first tab when the active tab
    // isn't in it, which includes a pinned one; pinned tabs are valid in focus
    // mode, so give activation back.
    tabs::TabInterface* active = model_->GetActiveTab();
    base::WeakPtr<tabs::TabInterface> pinned;
    if (active && active->IsPinned()) {
      pinned = active->GetWeakPtr();
    }
    model_->EnterFocusMode(*space);
    if (pinned) {
      const int index = model_->GetIndexOfTab(pinned.get());
      if (index != TabStripModel::kNoTab) {
        model_->ActivateTabAt(index);
      }
    }
  }
}

void ArgonSpaces::ArchiveIdleTabs() {
  if (model_->closing_all()) {
    return;
  }
  std::vector<base::WeakPtr<tabs::TabInterface>> idle;
  for (int i = 0; i < model_->count(); ++i) {
    tabs::TabInterface* tab = model_->GetTabAtIndex(i);
    const std::optional<base::TimeDelta> left = TimeUntilArchive(tab);
    if (left && !left->is_positive()) {
      idle.push_back(tab->GetWeakPtr());
    }
  }
  // The active tab is never archived, so the window never empties.
  for (const auto& tab : idle) {
    const int index = tab ? model_->GetIndexOfTab(tab.get())
                          : TabStripModel::kNoTab;
    if (index != TabStripModel::kNoTab) {
      model_->CloseWebContentsAt(index,
                                 TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
    }
  }
}

tab_groups::TabGroupId ArgonSpaces::NewSpace(const std::vector<int>& indices,
                                             std::u16string name) {
  const tab_groups::TabGroupId space = model_->AddToNewGroup(indices);
  const TabGroup* group = model_->group_model()->GetTabGroup(space);
  model_->ChangeTabGroupVisuals(
      space, tab_groups::TabGroupVisualData(std::move(name),
                                            group->visual_data()->color()));
  return space;
}

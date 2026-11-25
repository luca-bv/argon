// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_spaces.h"

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"

namespace {

// ponytail: fixed names until spaces can be renamed from Argon's own UI; the
// tab group editor (right-click a group) renames them meanwhile.
constexpr char16_t kFirstSpaceName[] = u"Home";

bool Enabled(const TabStripModel* model) {
  return model->SupportsTabGroups() &&
         base::FeatureList::IsEnabled(features::kTabGroupsFocusing);
}

}  // namespace

ArgonSpaces::ArgonSpaces(BrowserWindowInterface* browser)
    : browser_(browser), model_(browser->GetTabStripModel()) {
  model_->AddObserver(this);
  ScheduleEnforce();
}

ArgonSpaces::~ArgonSpaces() {
  model_->RemoveObserver(this);
}

std::vector<tab_groups::TabGroupId> ArgonSpaces::List() const {
  std::vector<tab_groups::TabGroupId> spaces;
  if (!Enabled(model_)) {
    return spaces;
  }
  for (int i = 0; i < model_->count(); ++i) {
    const std::optional<tab_groups::TabGroupId> group =
        model_->GetTabAtIndex(i)->GetGroup();
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
  chrome::ExecuteCommand(browser_, IDC_NEW_TAB);
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

void ArgonSpaces::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
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

tab_groups::TabGroupId ArgonSpaces::NewSpace(const std::vector<int>& indices,
                                             std::u16string name) {
  const tab_groups::TabGroupId space = model_->AddToNewGroup(indices);
  const TabGroup* group = model_->group_model()->GetTabGroup(space);
  model_->ChangeTabGroupVisuals(
      space, tab_groups::TabGroupVisualData(std::move(name),
                                            group->visual_data()->color()));
  return space;
}

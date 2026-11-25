// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_SPACES_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_SPACES_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"

class BrowserWindowInterface;
class TabStripModel;

namespace tabs {
class TabInterface;
}  // namespace tabs

// Spaces (docs/notes.md, "Spaces"), built on Chromium's tab group focus mode:
// a space is a tab group and the current space is the focused group. This
// keeps every unpinned tab in a space and one space focused at all times.
// Chromium exits focus mode for many reasons (activating a tab in another
// group, closing a group, new tabs, ...); each exit is turned into a switch to
// the active tab's space, or back to the last one, instead of patching the
// exits one by one. Pinned tabs show in every space.
class ArgonSpaces : public TabStripModelObserver {
 public:
  explicit ArgonSpaces(BrowserWindowInterface* browser);
  ArgonSpaces(const ArgonSpaces&) = delete;
  ArgonSpaces& operator=(const ArgonSpaces&) = delete;
  ~ArgonSpaces() override;

  // Spaces in tab strip order.
  std::vector<tab_groups::TabGroupId> List() const;
  // Switches to `space`, back to the tab that was active there.
  void Switch(const tab_groups::TabGroupId& space);
  // Opens a new tab in a new space and switches to it.
  void Create();

  // TabStripModelObserver:
  void OnTabStripModelChanged(TabStripModel* tab_strip_model,
                              const TabStripModelChange& change,
                              const TabStripSelectionChange& selection) override;
  void OnTabPinnedStateChanged(tabs::TabInterface* tab, int index) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;

 private:
  // The model can't be changed from inside its own notifications, so fixes
  // run after the current task.
  void ScheduleEnforce();
  void Enforce();
  tab_groups::TabGroupId NewSpace(const std::vector<int>& indices,
                                  std::u16string name);

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<TabStripModel> model_;

  // The space focused before focus mode was last exited.
  std::optional<tab_groups::TabGroupId> last_space_;
  // Each space's last active tab.
  std::map<tab_groups::TabGroupId, base::WeakPtr<tabs::TabInterface>>
      last_active_;

  bool enforce_pending_ = false;
  base::WeakPtrFactory<ArgonSpaces> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_SPACES_H_

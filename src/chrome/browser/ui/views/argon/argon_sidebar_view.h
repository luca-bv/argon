// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_SIDEBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_SIDEBAR_VIEW_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/argon/argon_palette.h"
#include "chrome/browser/ui/views/argon/argon_spaces.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class BrowserWindowInterface;
class TabStripModel;

namespace views {
class BoxLayoutView;
}  // namespace views

// Argon's sidebar zone: draws the modules of a layout (argon_layout.h), top to
// bottom. It takes the place of the vertical tab strip's tab list inside
// VerticalTabStripRegionView, which still owns the width, collapse, resize and
// the window controls above it. Collapsed, it shows favicons only.
//
// The space is the focused tab group (argon_spaces.h). Until pins (Spaces
// stage 2) land, kept tabs are Chromium-pinned tabs, and loose tabs are the
// space's other tabs.
class ArgonSidebarView : public views::View, public TabStripModelObserver {
  METADATA_HEADER(ArgonSidebarView, views::View)

 public:
  explicit ArgonSidebarView(BrowserWindowInterface* browser);
  ArgonSidebarView(const ArgonSidebarView&) = delete;
  ArgonSidebarView& operator=(const ArgonSidebarView&) = delete;
  ~ArgonSidebarView() override;

  // views::View:
  void OnThemeChanged() override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(TabStripModel* tab_strip_model,
                              const TabStripModelChange& change,
                              const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;
  void OnTabPinnedStateChanged(tabs::TabInterface* tab, int index) override;
  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void OnTabGroupFocusChanged(
      std::optional<tab_groups::TabGroupId> new_focused_group_id,
      std::optional<tab_groups::TabGroupId> old_focused_group_id) override;

 private:
  // Modules are rebuilt from the model after the current task, so a row is
  // never destroyed while its own click handler runs (closing a tab notifies
  // synchronously), and bursts of changes during page loads coalesce.
  void ScheduleRebuild();
  void Rebuild();
  // Updates every tab row in place (titles, favicons, archive timers).
  void RefreshRows();

  // Whether the tab at `index` is shown in the current space.
  bool InSpace(int index) const;
  // Adds one row per tab in the space that is (or isn't) kept. Returns whether
  // it added any.
  bool AddTabRows(views::View* parent, bool kept);
  void AddNewTabRow(views::BoxLayoutView* row, bool any_loose);
  void AddSpaceButtons(views::BoxLayoutView* row);

  void Activate(base::WeakPtr<tabs::TabInterface> tab);
  void Close(base::WeakPtr<tabs::TabInterface> tab);
  void CloseLooseTabs();
  void NewTab();

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<TabStripModel> model_;
  const std::unique_ptr<ArgonSpaces> spaces_;

  raw_ptr<views::View> contents_ = nullptr;
  ArgonPalette palette_;
  bool compact_ = false;

  base::RepeatingTimer refresh_timer_;
  bool rebuild_pending_ = false;
  base::WeakPtrFactory<ArgonSidebarView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_SIDEBAR_VIEW_H_

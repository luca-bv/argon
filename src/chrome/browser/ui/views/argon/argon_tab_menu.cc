// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_tab_menu.h"

#include <memory>
#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/argon/argon_pins.h"
#include "chrome/browser/ui/views/argon/argon_spaces.h"
#include "chrome/browser/ui/views/argon/argon_window_set.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

enum Command {
  kPinInSpace,
  kUnpin,
  kBackToPinnedUrl,
  kUseCurrentUrl,
  kAddToFavorites,
  kRemoveFromFavorites,
  kClose,
};

// The URL a tab would be pinned at.
GURL CurrentUrl(content::WebContents* contents) {
  const GURL& committed = contents->GetLastCommittedURL();
  return committed.is_empty() ? contents->GetVisibleURL() : committed;
}

class TabMenu : public ui::SimpleMenuModel::Delegate {
 public:
  TabMenu(BrowserWindowInterface* browser, base::WeakPtr<tabs::TabInterface> tab)
      : browser_id_(browser->GetSessionID()), tab_(std::move(tab)) {
    content::WebContents* contents = tab_->GetContents();
    if (tab_->IsPinned()) {
      model_.AddItem(kRemoveFromFavorites, u"Remove from Favorites");
    } else {
      if (argon_pins::IsPin(contents)) {
        model_.AddItem(kUnpin, u"Unpin");
        model_.AddItem(kBackToPinnedUrl, u"Go back to pinned page");
        model_.AddItem(kUseCurrentUrl, u"Pin this page instead");
      } else if (tab_->GetGroup()) {
        model_.AddItem(kPinInSpace, u"Pin in this space");
      }
      model_.AddItem(kAddToFavorites, u"Add to Favorites");
    }
    // A space pin's × only resets it, so no Close here.
    if (!argon_pins::IsPin(contents)) {
      model_.AddSeparator(ui::NORMAL_SEPARATOR);
      model_.AddItem(kClose, u"Close");
    }
  }
  TabMenu(const TabMenu&) = delete;
  TabMenu& operator=(const TabMenu&) = delete;
  ~TabMenu() override = default;

  void Run(views::Widget* parent,
           const gfx::Point& point,
           ui::mojom::MenuSourceType source_type) {
    runner_ = std::make_unique<views::MenuRunner>(
        &model_, views::MenuRunner::CONTEXT_MENU);
    runner_->RunMenuAt(parent, nullptr, gfx::Rect(point, gfx::Size()),
                       views::MenuAnchorPosition::kTopLeft, source_type);
  }

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override {
    BrowserWindowInterface* browser =
        BrowserWindowInterface::FromSessionID(browser_id_);
    if (!browser || !tab_) {
      return;
    }
    TabStripModel* model = browser->GetTabStripModel();
    const int index = model->GetIndexOfTab(tab_.get());
    if (index == TabStripModel::kNoTab) {
      return;
    }
    content::WebContents* contents = tab_->GetContents();
    switch (command_id) {
      case kPinInSpace:
        argon_pins::SetPin(contents, CurrentUrl(contents));
        break;
      case kUnpin:
        argon_pins::SetPin(contents, GURL());
        break;
      case kBackToPinnedUrl:
        if (argon_pins::IsPin(contents)) {
          contents->GetController().LoadURL(
              argon_pins::PinnedUrl(contents), content::Referrer(),
              ui::PAGE_TRANSITION_AUTO_BOOKMARK, std::string());
        }
        return;
      case kUseCurrentUrl:
        argon_pins::SetPin(contents, CurrentUrl(contents));
        break;
      case kAddToFavorites:
        // ArgonSpaces drops the space pin when a tab becomes a global pin.
        model->SetTabPinned(index, true);
        return;
      case kRemoveFromFavorites:
        // ArgonSpaces moves it into the current space.
        model->SetTabPinned(index, false);
        return;
      case kClose:
        model->CloseWebContentsAt(
            index, TabCloseTypes::CLOSE_USER_GESTURE |
                       TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
        return;
    }
    if (ArgonSpaces* spaces = ArgonWindowSet::Get()->SpacesFor(browser)) {
      spaces->PinsChanged();
    }
  }

 private:
  const SessionID browser_id_;
  const base::WeakPtr<tabs::TabInterface> tab_;
  ui::SimpleMenuModel model_{this};
  std::unique_ptr<views::MenuRunner> runner_;
};

// ponytail: kept until the next menu opens; the runner must outlive its menu.
std::unique_ptr<TabMenu>& LastMenu() {
  static base::NoDestructor<std::unique_ptr<TabMenu>> menu;
  return *menu;
}

}  // namespace

void ShowArgonTabMenu(views::View* source,
                      BrowserWindowInterface* browser,
                      base::WeakPtr<tabs::TabInterface> tab,
                      const gfx::Point& point,
                      ui::mojom::MenuSourceType source_type) {
  if (!tab || !source->GetWidget()) {
    return;
  }
  LastMenu() = std::make_unique<TabMenu>(browser, std::move(tab));
  LastMenu()->Run(source->GetWidget(), point, source_type);
}

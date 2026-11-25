// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_sidebar_view.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/views/argon/argon_font.h"
#include "chrome/browser/ui/views/argon/argon_layout.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_utils.h"

namespace {

// Sizes from the handoff's tokens (Regular density).
constexpr int kRowHeight = 30;
constexpr int kRowRadius = 7;
constexpr int kRowPadding = 8;
constexpr int kRowGap = 1;
constexpr int kSectionGap = 14;
constexpr int kIconTitleGap = 9;
constexpr int kFaviconSize = 16;
constexpr int kCloseSize = 24;
constexpr int kCloseIconSize = 10;
// Narrower than this (the collapsed strip, ~32px; the narrowest expanded one is
// ~100px), rows show only their favicon.
constexpr int kCompactWidth = 64;

// Shown for a space whose group has no title.
constexpr char16_t kDefaultSpaceName[] = u"Home";

// Type sizes from the handoff: 13 for rows, 12 for the space name and Clear.
constexpr int kTextSize = 13;
constexpr int kSmallTextSize = 12;

// One tab: favicon and title; loose tabs also get a close button, shown while
// the row is hovered or active. Compact rows show the favicon alone.
class ArgonTabRow : public views::Button {
  METADATA_HEADER(ArgonTabRow, views::Button)

 public:
  ArgonTabRow(tabs::TabInterface* tab,
              bool active,
              bool compact,
              const ArgonPalette& palette,
              PressedCallback open,
              PressedCallback close)
      : views::Button(std::move(open)),
        tab_(tab->GetWeakPtr()),
        active_(active),
        compact_(compact),
        palette_(palette) {
    SetNotifyEnterExitOnChild(true);
    // Like real tabs: activate on press, so a click still lands when the row
    // is rebuilt before the mouse comes up.
    button_controller()->set_notify_action(
        views::ButtonController::NotifyAction::kOnPress);

    // Compact rows are barely wider than the favicon, so they center it with
    // no padding.
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal,
        compact ? gfx::Insets()
                : gfx::Insets::TLBR(0, kRowPadding, 0, close ? 3 : kRowPadding),
        kIconTitleGap));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_main_axis_alignment(
        compact ? views::BoxLayout::MainAxisAlignment::kCenter
                : views::BoxLayout::MainAxisAlignment::kStart);
    layout->set_minimum_cross_axis_size(kRowHeight);

    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetImageSize(gfx::Size(kFaviconSize, kFaviconSize));
    if (compact) {
      UpdateFromTab();
      UpdateLook();
      return;
    }

    title_ = AddChildView(std::make_unique<views::Label>());
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetElideBehavior(gfx::ELIDE_TAIL);
    title_->SetAutoColorReadabilityEnabled(false);
    title_->SetFontList(ArgonFont(kTextSize,
                                  active ? gfx::Font::Weight::SEMIBOLD
                                         : gfx::Font::Weight::NORMAL));
    layout->SetFlexForView(title_, 1);

    if (close) {
      close_ = AddChildView(views::CreateVectorImageButton(std::move(close)));
      views::SetImageFromVectorIconWithColor(
          close_, vector_icons::kCloseIcon, kCloseIconSize,
          views::IconColors(palette_.muted, palette_.muted));
      close_->SetPreferredSize(gfx::Size(kCloseSize, kCloseSize));
      close_->GetViewAccessibility().SetName(u"Close tab");
    }
    UpdateFromTab();
    UpdateLook();
  }
  ArgonTabRow(const ArgonTabRow&) = delete;
  ArgonTabRow& operator=(const ArgonTabRow&) = delete;
  ~ArgonTabRow() override = default;

  const tabs::TabInterface* tab() const { return tab_.get(); }

  // Title and favicon, which change while a page loads.
  void UpdateFromTab() {
    if (!tab_) {
      return;
    }
    TabUIHelper* helper = TabUIHelper::From(tab_.get());
    const std::u16string title = helper->GetTitle();
    GetViewAccessibility().SetName(title);
    if (compact_) {
      SetTooltipText(title);
    }
    if (title_) {
      title_->SetText(title);
    }
    icon_->SetImage(helper->GetFavicon());
  }

 protected:
  void StateChanged(ButtonState old_state) override {
    views::Button::StateChanged(old_state);
    UpdateLook();
  }

 private:
  void UpdateLook() {
    const bool hovered = GetState() == STATE_HOVERED ||
                         GetState() == STATE_PRESSED || IsMouseHovered();
    if (active_ || hovered) {
      SetBackground(views::CreateRoundedRectBackground(
          active_ ? palette_.active : palette_.hover, kRowRadius));
    } else {
      SetBackground(nullptr);
    }
    if (title_) {
      title_->SetEnabledColor(active_ || hovered ? palette_.ink
                                                 : palette_.secondary);
    }
    if (close_) {
      close_->SetVisible(active_ || hovered);
    }
  }

  const base::WeakPtr<tabs::TabInterface> tab_;
  const bool active_;
  const bool compact_;
  const ArgonPalette palette_;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::ImageButton> close_ = nullptr;
};

BEGIN_METADATA(ArgonTabRow)
END_METADATA

std::unique_ptr<views::View> MakeDivider(SkColor color) {
  auto divider = std::make_unique<views::View>();
  divider->SetPreferredSize(gfx::Size(0, 1));
  divider->SetBackground(views::CreateSolidBackground(color));
  return divider;
}

std::unique_ptr<views::BoxLayoutView> MakeList() {
  auto list = std::make_unique<views::BoxLayoutView>();
  list->SetOrientation(views::BoxLayout::Orientation::kVertical);
  list->SetBetweenChildSpacing(kRowGap);
  list->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStretch);
  return list;
}

// LabelButton keeps label() protected, and has no other way to set the font.
class TextButton : public views::LabelButton {
 public:
  using LabelButton::LabelButton;
  using LabelButton::label;
};

std::unique_ptr<views::LabelButton> MakeTextButton(
    views::Button::PressedCallback callback,
    std::u16string text,
    int size,
    const ArgonPalette& palette) {
  auto button =
      std::make_unique<TextButton>(std::move(callback), std::move(text));
  button->SetTextColor(views::Button::STATE_NORMAL, palette.muted);
  button->SetTextColor(views::Button::STATE_HOVERED, palette.ink);
  button->SetTextColor(views::Button::STATE_PRESSED, palette.ink);
  button->label()->SetFontList(ArgonFont(size, gfx::Font::Weight::NORMAL));
  button->label()->SetAutoColorReadabilityEnabled(false);
  button->SetMinSize(gfx::Size(0, kRowHeight));
  button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, kRowPadding)));
  return button;
}

}  // namespace

ArgonSidebarView::ArgonSidebarView(BrowserWindowInterface* browser)
    : browser_(browser),
      model_(browser->GetTabStripModel()),
      spaces_(std::make_unique<ArgonSpaces>(browser)) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto* scroll = AddChildView(std::make_unique<views::ScrollView>());
  scroll->SetHorizontalScrollBarMode(views::ScrollView::ScrollBarMode::kDisabled);
  scroll->SetBackgroundColor(std::nullopt);
  scroll->SetDrawOverflowIndicator(false);
  // Bounded, so the contents get the viewport's width and their own height,
  // and scroll once the tabs outgrow the window.
  scroll->ClipHeightTo(0, std::numeric_limits<int>::max());

  auto* contents =
      scroll->SetContents(std::make_unique<views::BoxLayoutView>());
  contents->SetOrientation(views::BoxLayout::Orientation::kVertical);
  contents->SetBetweenChildSpacing(kSectionGap);
  contents->SetInsideBorderInsets(gfx::Insets::TLBR(6, 0, 18, 0));
  contents->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);
  contents_ = contents;

  model_->AddObserver(this);
  Rebuild();
}

ArgonSidebarView::~ArgonSidebarView() {
  model_->RemoveObserver(this);
}

void ArgonSidebarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  palette_ = ArgonPalette(GetColorProvider()->GetColor(ui::kColorFrameActive));
  ScheduleRebuild();
}

void ArgonSidebarView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  const bool compact = width() < kCompactWidth;
  if (compact != compact_) {
    compact_ = compact;
    ScheduleRebuild();
  }
}

void ArgonSidebarView::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  ScheduleRebuild();
}

// Fires over and over while pages load: update that tab's row in place, so
// rows under the pointer aren't torn down mid-click.
void ArgonSidebarView::OnTabChangedAt(tabs::TabInterface* tab,
                                      TabChangeType change_type) {
  for (views::View* list : contents_->children()) {
    for (views::View* child : list->children()) {
      if (auto* row = views::AsViewClass<ArgonTabRow>(child);
          row && row->tab() == tab) {
        row->UpdateFromTab();
        return;
      }
    }
  }
}

void ArgonSidebarView::OnTabPinnedStateChanged(tabs::TabInterface* tab,
                                               int index) {
  ScheduleRebuild();
}

void ArgonSidebarView::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  ScheduleRebuild();
}

void ArgonSidebarView::OnTabGroupChanged(const TabGroupChange& change) {
  ScheduleRebuild();
}

void ArgonSidebarView::OnTabGroupFocusChanged(
    std::optional<tab_groups::TabGroupId> new_focused_group_id,
    std::optional<tab_groups::TabGroupId> old_focused_group_id) {
  ScheduleRebuild();
}

void ArgonSidebarView::ScheduleRebuild() {
  if (rebuild_pending_) {
    return;
  }
  rebuild_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ArgonSidebarView::Rebuild,
                                weak_factory_.GetWeakPtr()));
}

// ponytail: rebuilds every module on structural changes (tabs added, closed,
// moved, pinned, grouped, activated); diff rows if long tab lists make this
// show up in profiles.
void ArgonSidebarView::Rebuild() {
  rebuild_pending_ = false;
  contents_->RemoveAllChildViews();

  bool any_loose = false;
  for (int i = 0; i < model_->count(); ++i) {
    any_loose |= !model_->IsTabPinned(i) && InSpace(i);
  }

  // A divider is drawn only once something follows the previous one, so an
  // empty module never leaves two dividers (or a leading one) behind.
  bool divider_pending = false;
  bool drew_since_divider = false;
  auto add = [&](std::unique_ptr<views::View> view) {
    if (divider_pending) {
      contents_->AddChildView(MakeDivider(palette_.line));
      divider_pending = false;
    }
    contents_->AddChildView(std::move(view));
    drew_since_divider = true;
  };

  for (const ArgonModule module : kQuietSidebar) {
    switch (module) {
      case ArgonModule::kSpaceName: {
        if (compact_) {
          break;
        }
        std::u16string name = kDefaultSpaceName;
        if (const std::optional<tab_groups::TabGroupId> focused =
                model_->GetFocusedGroup()) {
          const TabGroup* group = model_->group_model()->GetTabGroup(*focused);
          if (group && !group->visual_data()->title().empty()) {
            name = group->visual_data()->title();
          }
        }
        auto label = std::make_unique<views::Label>(name);
        label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
        label->SetFontList(ArgonFont(kSmallTextSize, gfx::Font::Weight::SEMIBOLD));
        label->SetAutoColorReadabilityEnabled(false);
        label->SetEnabledColor(palette_.muted);
        label->SetBorder(
            views::CreateEmptyBorder(gfx::Insets::VH(0, kRowPadding)));
        add(std::move(label));
        break;
      }
      case ArgonModule::kDivider:
        divider_pending = drew_since_divider;
        drew_since_divider = false;
        break;
      case ArgonModule::kPinned:
      case ArgonModule::kLoose: {
        auto list = MakeList();
        if (AddTabRows(list.get(), module == ArgonModule::kPinned)) {
          add(std::move(list));
        }
        break;
      }
      case ArgonModule::kNewTab: {
        auto row = std::make_unique<views::BoxLayoutView>();
        AddNewTabRow(row.get(), any_loose);
        add(std::move(row));
        break;
      }
      case ArgonModule::kSpaces: {
        // ponytail: no switcher while collapsed until the edge pill exists.
        if (compact_) {
          break;
        }
        auto row = std::make_unique<views::BoxLayoutView>();
        AddSpaceButtons(row.get());
        add(std::move(row));
        break;
      }
    }
  }
  PreferredSizeChanged();
}

bool ArgonSidebarView::InSpace(int index) const {
  // Focus mode hides the other groups' tabs; so does the sidebar. Kept tabs
  // show in every space, and the active tab always shows, so it can't end up
  // unreachable outside the space.
  const std::optional<tab_groups::TabGroupId> focused =
      model_->GetFocusedGroup();
  return model_->IsTabPinned(index) || !focused ||
         index == model_->active_index() ||
         model_->GetTabGroupForTab(index) == focused;
}

bool ArgonSidebarView::AddTabRows(views::View* parent, bool kept) {
  bool added = false;
  for (int i = 0; i < model_->count(); ++i) {
    if (model_->IsTabPinned(i) != kept || !InSpace(i)) {
      continue;
    }
    tabs::TabInterface* tab = model_->GetTabAtIndex(i);
    auto weak_tab = tab->GetWeakPtr();
    auto open = base::BindRepeating(&ArgonSidebarView::Activate,
                                    weak_factory_.GetWeakPtr(), weak_tab);
    views::Button::PressedCallback close;
    if (!kept) {
      close = base::BindRepeating(&ArgonSidebarView::Close,
                                  weak_factory_.GetWeakPtr(), weak_tab);
    }
    parent->AddChildView(std::make_unique<ArgonTabRow>(
        tab, i == model_->active_index(), compact_, palette_, std::move(open),
        std::move(close)));
    added = true;
  }
  return added;
}

void ArgonSidebarView::AddNewTabRow(views::BoxLayoutView* row,
                                    bool any_loose) {
  row->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  row->SetMainAxisAlignment(compact_
                                ? views::BoxLayout::MainAxisAlignment::kCenter
                                : views::BoxLayout::MainAxisAlignment::kStart);
  auto* new_tab = row->AddChildView(MakeTextButton(
      base::BindRepeating(&ArgonSidebarView::NewTab, base::Unretained(this)),
      compact_ ? u"+" : u"+ New", kTextSize, palette_));
  new_tab->SetTooltipText(u"New tab");
  if (compact_ || !any_loose) {
    return;
  }
  row->SetFlexForView(new_tab, 1);
  auto* clear = row->AddChildView(MakeTextButton(
      base::BindRepeating(&ArgonSidebarView::CloseLooseTabs,
                          base::Unretained(this)),
      u"Clear", kSmallTextSize, palette_));
  clear->SetTooltipText(u"Close every loose tab in this space");
}

void ArgonSidebarView::AddSpaceButtons(views::BoxLayoutView* row) {
  row->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  row->SetBetweenChildSpacing(2);
  const std::optional<tab_groups::TabGroupId> current =
      model_->GetFocusedGroup();
  for (const tab_groups::TabGroupId& space : spaces_->List()) {
    std::u16string name = kDefaultSpaceName;
    if (const TabGroup* group = model_->group_model()->GetTabGroup(space);
        group && !group->visual_data()->title().empty()) {
      name = group->visual_data()->title();
    }
    // ponytail: the name's first letter stands in for space icons.
    auto* button = row->AddChildView(MakeTextButton(
        base::BindRepeating(&ArgonSpaces::Switch,
                            base::Unretained(spaces_.get()), space),
        name.substr(0, 1), kTextSize, palette_));
    button->SetTooltipText(name);
    button->GetViewAccessibility().SetName(name);
    if (space == current) {
      button->SetTextColor(views::Button::STATE_NORMAL, palette_.ink);
      button->SetBackground(
          views::CreateRoundedRectBackground(palette_.active, kRowRadius));
    }
  }
  auto* add = row->AddChildView(MakeTextButton(
      base::BindRepeating(&ArgonSpaces::Create, base::Unretained(spaces_.get())),
      u"+", kTextSize, palette_));
  add->SetTooltipText(u"New space");
}

void ArgonSidebarView::Activate(base::WeakPtr<tabs::TabInterface> tab) {
  if (!tab) {
    return;
  }
  const int index = model_->GetIndexOfTab(tab.get());
  if (index != TabStripModel::kNoTab) {
    model_->ActivateTabAt(index, TabStripUserGestureDetails(
                                     TabStripUserGestureDetails::GestureType::kMouse));
  }
}

// Closed tabs stay reopenable with Cmd+Shift+T (historical tab entries).
void ArgonSidebarView::Close(base::WeakPtr<tabs::TabInterface> tab) {
  if (!tab) {
    return;
  }
  const int index = model_->GetIndexOfTab(tab.get());
  if (index != TabStripModel::kNoTab) {
    model_->CloseWebContentsAt(index, TabCloseTypes::CLOSE_USER_GESTURE |
                                          TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  }
}

void ArgonSidebarView::CloseLooseTabs() {
  std::vector<base::WeakPtr<tabs::TabInterface>> loose;
  for (int i = 0; i < model_->count(); ++i) {
    if (!model_->IsTabPinned(i) && InSpace(i)) {
      loose.push_back(model_->GetTabAtIndex(i)->GetWeakPtr());
    }
  }
  // Closing a window's last tab closes the window; Clear only empties the space.
  if (static_cast<int>(loose.size()) == model_->count()) {
    NewTab();
  }
  for (const auto& tab : loose) {
    Close(tab);
  }
}

void ArgonSidebarView::NewTab() {
  chrome::ExecuteCommand(browser_, IDC_NEW_TAB);
}

BEGIN_METADATA(ArgonSidebarView)
END_METADATA

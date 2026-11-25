// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_sidebar_view.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/views/argon/argon_command_bar.h"
#include "chrome/browser/ui/views/argon/argon_font.h"
#include "chrome/browser/ui/views/argon/argon_layout.h"
#include "chrome/browser/ui/views/argon/argon_toast.h"
#include "chrome/browser/ui/views/argon/argon_window_set.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
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
constexpr int kTimerTextSize = 11;
constexpr int kNavIconSize = 16;
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
      // Loose tabs: time left before they're archived, replaced by the close
      // button on hover.
      timer_ = AddChildView(std::make_unique<views::Label>());
      timer_->SetFontList(
          ArgonFont(kTimerTextSize, gfx::Font::Weight::NORMAL));
      timer_->SetAutoColorReadabilityEnabled(false);
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
    if (timer_) {
      left_ = ArgonSpaces::TimeUntilArchive(tab_.get());
      if (left_) {
        const base::TimeDelta left = std::max(*left_, base::TimeDelta());
        // Nm under an hour, Nh under two days, else Nd.
        timer_->SetText(
            left < base::Hours(1)
                ? base::NumberToString16(std::max<int64_t>(left.InMinutes(), 1)) +
                      u"m"
            : left < base::Hours(48)
                ? base::NumberToString16(left.InHours()) + u"h"
                : base::NumberToString16(left.InDays()) + u"d");
        timer_->SetEnabledColor(left < base::Hours(2) ? palette_.warning
                                                      : palette_.muted);
      }
      UpdateLook();
    }
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
    if (timer_) {
      timer_->SetVisible(!active_ && !hovered && left_.has_value());
    }
  }

  const base::WeakPtr<tabs::TabInterface> tab_;
  const bool active_;
  const bool compact_;
  const ArgonPalette palette_;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> timer_ = nullptr;
  raw_ptr<views::ImageButton> close_ = nullptr;
  std::optional<base::TimeDelta> left_;
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

  // ⌃1–⌃9 go to the first nine spaces.
  for (int i = 0; i < 9; ++i) {
    AddAccelerator(ui::Accelerator(static_cast<ui::KeyboardCode>(ui::VKEY_1 + i),
                                   ui::EF_CONTROL_DOWN));
  }

  model_->AddObserver(this);
  // Spaces in the set's other windows show here too.
  window_set_subscription_ = ArgonWindowSet::Get()->Subscribe(
      base::BindRepeating(&ArgonSidebarView::OnWindowSetChanged,
                          weak_factory_.GetWeakPtr()));
  Rebuild();
  // Archive timers count down by the minute.
  refresh_timer_.Start(FROM_HERE, base::Minutes(1), this,
                       &ArgonSidebarView::RefreshRows);
}

ArgonSidebarView::~ArgonSidebarView() {
  model_->RemoveObserver(this);
}

bool ArgonSidebarView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const size_t index = accelerator.key_code() - ui::VKEY_1;
  const std::vector<ArgonWindowSet::Space> spaces =
      ArgonWindowSet::Get()->List(browser_);
  if (index >= spaces.size()) {
    return false;  // No such space: the page may use the key.
  }
  ArgonWindowSet::Get()->Switch(browser_, spaces[index]);
  return true;
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
  if (tab == model_->GetActiveTab()) {
    UpdateNavigation();
  }
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

void ArgonSidebarView::RefreshRows() {
  for (views::View* list : contents_->children()) {
    for (views::View* child : list->children()) {
      if (auto* row = views::AsViewClass<ArgonTabRow>(child)) {
        row->UpdateFromTab();
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

void ArgonSidebarView::OnWindowSetChanged(BrowserWindowInterface* browser) {
  // This window's own changes already reach it as a TabStripModelObserver.
  if (browser != browser_) {
    ScheduleRebuild();
  }
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
  back_ = nullptr;
  forward_ = nullptr;
  address_ = nullptr;
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
      case ArgonModule::kNavigation: {
        // ponytail: hidden while collapsed; the collapsed strip has no room.
        if (compact_) {
          break;
        }
        auto row = std::make_unique<views::BoxLayoutView>();
        AddNavigationRow(row.get());
        add(std::move(row));
        break;
      }
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
  UpdateNavigation();
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

void ArgonSidebarView::AddNavigationRow(views::BoxLayoutView* row) {
  row->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  row->SetBetweenChildSpacing(2);
  const auto add_button = [&](const gfx::VectorIcon& icon, int command,
                              std::u16string name) {
    auto* button = row->AddChildView(views::CreateVectorImageButton(
        base::BindRepeating(&ArgonSidebarView::RunCommand,
                            base::Unretained(this), command)));
    // The second color is the disabled one.
    views::SetImageFromVectorIconWithColor(
        button, icon, kNavIconSize,
        views::IconColors(palette_.secondary, palette_.line));
    button->SetPreferredSize(gfx::Size(kCloseSize, kCloseSize));
    button->SetTooltipText(name);
    button->GetViewAccessibility().SetName(name);
    return button;
  };
  back_ = add_button(vector_icons::kArrowBackIcon, IDC_BACK, u"Back");
  forward_ =
      add_button(vector_icons::kArrowForwardIcon, IDC_FORWARD, u"Forward");
  add_button(vector_icons::kRefreshIcon, IDC_RELOAD, u"Reload");
  address_ = row->AddChildView(MakeTextButton(
      base::BindRepeating(&ArgonSidebarView::EditAddress,
                          base::Unretained(this)),
      std::u16string(), kSmallTextSize, palette_));
  // Elide the start, so the registrable domain stays visible: this is the only
  // place the page's origin shows without the toolbar.
  address_->SetElideBehavior(gfx::ELIDE_HEAD);
  address_->SetTooltipText(u"Edit address (\u2318L)");
  row->SetFlexForView(address_, 1);
}

void ArgonSidebarView::UpdateNavigation() {
  content::WebContents* contents = model_->GetActiveWebContents();
  if (!back_ || !contents) {
    return;
  }
  back_->SetEnabled(contents->GetController().CanGoBack());
  forward_->SetEnabled(contents->GetController().CanGoForward());
  // The site, as Arc shows it: host without "www.", or the whole URL when
  // there's no host.
  const GURL& url = contents->GetVisibleURL();
  std::string host(url.host());
  if (base::StartsWith(host, "www.")) {
    host.erase(0, 4);
  }
  address_->SetText(base::UTF8ToUTF16(host.empty() ? url.spec() : host));
}

void ArgonSidebarView::RunCommand(int command) {
  chrome::ExecuteCommand(browser_, command);
}

void ArgonSidebarView::EditAddress() {
  ShowArgonCommandBar(browser_, /*new_tab=*/false);
}

void ArgonSidebarView::AddSpaceButtons(views::BoxLayoutView* row) {
  row->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  row->SetBetweenChildSpacing(2);
  const std::optional<tab_groups::TabGroupId> current =
      model_->GetFocusedGroup();
  // Every space in the window set: other profiles' spaces live in their own
  // windows (argon_window_set.h).
  const std::vector<ArgonWindowSet::Space> spaces =
      ArgonWindowSet::Get()->List(browser_);
  for (size_t i = 0; i < spaces.size(); ++i) {
    const ArgonWindowSet::Space& space = spaces[i];
    std::u16string name = kDefaultSpaceName;
    // The space's accent is its tab group's color (the color picked in the
    // group editor); before the view has a theme, plain ink.
    SkColor accent = palette_.ink;
    if (const TabGroup* group = space.browser->GetTabStripModel()
                                    ->group_model()
                                    ->GetTabGroup(space.id)) {
      if (!group->visual_data()->title().empty()) {
        name = group->visual_data()->title();
      }
      if (const ui::ColorProvider* colors = GetColorProvider()) {
        accent = colors->GetColor(GetTabGroupTabStripColorId(
            group->visual_data()->color(), /*active_frame=*/true));
      }
    }
    // ponytail: the name's first letter stands in for space icons.
    auto* button = row->AddChildView(MakeTextButton(
        base::BindRepeating(&ArgonSidebarView::SwitchSpace,
                            base::Unretained(this), space),
        name.substr(0, 1), kTextSize, palette_));
    // "Name · ⌃N", as in the handoff.
    button->SetTooltipText(
        i < 9 ? name + u" \u00b7 \u2303" + base::NumberToString16(i + 1)
              : name);
    button->GetViewAccessibility().SetName(name);
    // The current space is filled with its accent, the others drawn in it.
    if (space.browser == browser_ && space.id == current) {
      for (const auto state : {views::Button::STATE_NORMAL,
                               views::Button::STATE_HOVERED,
                               views::Button::STATE_PRESSED}) {
        button->SetTextColor(state, SK_ColorWHITE);
      }
      button->SetBackground(
          views::CreateRoundedRectBackground(accent, kRowRadius));
    } else {
      button->SetTextColor(views::Button::STATE_NORMAL, accent);
    }
  }
  auto* add = row->AddChildView(MakeTextButton(
      base::BindRepeating(&ArgonSpaces::Create, base::Unretained(spaces_.get())),
      u"+", kTextSize, palette_));
  add->SetTooltipText(u"New space");
}

void ArgonSidebarView::SwitchSpace(const ArgonWindowSet::Space& space) {
  ArgonWindowSet::Get()->Switch(browser_, space);
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
bool ArgonSidebarView::CloseTab(base::WeakPtr<tabs::TabInterface> tab) {
  if (!tab) {
    return false;
  }
  const int index = model_->GetIndexOfTab(tab.get());
  if (index == TabStripModel::kNoTab) {
    return false;
  }
  model_->CloseWebContentsAt(index, TabCloseTypes::CLOSE_USER_GESTURE |
                                        TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
  return true;
}

void ArgonSidebarView::Close(base::WeakPtr<tabs::TabInterface> tab) {
  const std::optional<SessionID> newest = NewestClosedEntry();
  if (!CloseTab(tab)) {
    return;
  }
  // Only offer Undo for what reached the recently closed list (not a blank
  // tab, nothing in incognito, not a tab kept open by a "Leave site?" prompt).
  if (const int entries = ClosedEntriesSince(newest)) {
    ShowArgonToast(browser_, u"Closed tab",
                   base::BindOnce(&ArgonSidebarView::Reopen,
                                  weak_factory_.GetWeakPtr(), entries));
  }
}

std::optional<SessionID> ArgonSidebarView::NewestClosedEntry() {
  const sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (!service || service->entries().empty()) {
    return std::nullopt;
  }
  return service->entries().front()->id;
}

int ArgonSidebarView::ClosedEntriesSince(std::optional<SessionID> newest) {
  const sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser_->GetProfile());
  if (!service) {
    return 0;
  }
  // New entries go to the front.
  int count = 0;
  for (const auto& entry : service->entries()) {
    if (newest && entry->id == *newest) {
      break;
    }
    ++count;
  }
  return count;
}

// Each closed tab is its own entry in the recently closed list, newest first.
void ArgonSidebarView::Reopen(int count) {
  for (int i = 0; i < count; ++i) {
    chrome::ExecuteCommand(browser_, IDC_RESTORE_TAB);
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
  // (Not IDC_NEW_TAB, which opens the command bar.)
  if (static_cast<int>(loose.size()) == model_->count()) {
    chrome::NewTab(browser_, NewTabTypes::kNewTabCommand);
  }
  const std::optional<SessionID> newest = NewestClosedEntry();
  int closed = 0;
  for (const auto& tab : loose) {
    closed += CloseTab(tab);
  }
  const int entries = ClosedEntriesSince(newest);
  if (closed && entries) {
    ShowArgonToast(browser_,
                   closed == 1 ? std::u16string(u"Cleared 1 tab")
                               : u"Cleared " + base::NumberToString16(closed) +
                                     u" tabs",
                   base::BindOnce(&ArgonSidebarView::Reopen,
                                  weak_factory_.GetWeakPtr(), entries));
  }
}

void ArgonSidebarView::NewTab() {
  chrome::ExecuteCommand(browser_, IDC_NEW_TAB);
}

BEGIN_METADATA(ArgonSidebarView)
END_METADATA

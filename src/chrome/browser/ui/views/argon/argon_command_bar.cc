// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_command_bar.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/argon/argon_font.h"
#include "chrome/browser/ui/views/argon/argon_palette.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

// Sizes from the handoff.
constexpr int kWidth = 600;
constexpr int kCornerRadius = 16;
constexpr int kRowHeight = 40;
constexpr int kRowRadius = 9;
constexpr int kMaxTabs = 6;
constexpr int kInputSize = 18;
constexpr int kTitleSize = 14;
constexpr int kMetaSize = 12;
constexpr int kHintSize = 11;
// The panel's top edge, as a share of the window's height.
constexpr int kTopPercent = 14;

struct CommandResult {
  std::u16string title;
  std::u16string meta;
  base::RepeatingClosure run;
};

std::u16string SpaceName(const TabStripModel* model,
                         const tab_groups::TabGroupId& space) {
  const TabGroup* group = model->group_model()->GetTabGroup(space);
  return group && !group->visual_data()->title().empty()
             ? group->visual_data()->title()
             : u"Space";
}

std::unique_ptr<views::Label> MakeLabel(const std::u16string& text,
                                        int size,
                                        SkColor color) {
  auto label = std::make_unique<views::Label>(text);
  label->SetFontList(ArgonFont(size, gfx::Font::Weight::NORMAL));
  label->SetAutoColorReadabilityEnabled(false);
  label->SetEnabledColor(color);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetElideBehavior(gfx::ELIDE_TAIL);
  return label;
}

// One result: title, and its kind or space on the right.
class ResultRow : public views::Button {
  METADATA_HEADER(ResultRow, views::Button)

 public:
  ResultRow(const CommandResult& result,
            const ArgonPalette& palette,
            PressedCallback callback)
      : views::Button(std::move(callback)), palette_(palette) {
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, gfx::Insets::VH(0, 14),
        10));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    layout->set_minimum_cross_axis_size(kRowHeight);
    auto* title =
        AddChildView(MakeLabel(result.title, kTitleSize, palette.ink));
    layout->SetFlexForView(title, 1);
    AddChildView(MakeLabel(result.meta, kMetaSize, palette.muted));
    GetViewAccessibility().SetName(result.title);
  }
  ResultRow(const ResultRow&) = delete;
  ResultRow& operator=(const ResultRow&) = delete;
  ~ResultRow() override = default;

  void SetSelected(bool selected) {
    SetBackground(selected ? views::CreateRoundedRectBackground(
                                 palette_.active, kRowRadius)
                           : nullptr);
  }

 private:
  const ArgonPalette palette_;
};

BEGIN_METADATA(ResultRow)
END_METADATA

class CommandBarView : public views::View, public views::TextfieldController {
  METADATA_HEADER(CommandBarView, views::View)

 public:
  CommandBarView(BrowserWindowInterface* browser, bool new_tab)
      : browser_(browser),
        model_(browser->GetTabStripModel()),
        new_tab_(new_tab) {
    SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical));

    textfield_ = AddChildView(std::make_unique<views::Textfield>());
    textfield_->set_controller(this);
    textfield_->SetFontList(
        ArgonFont(kInputSize, gfx::Font::Weight::NORMAL));
    textfield_->SetPlaceholderText(
        u"Search tabs, type a URL, or run a command");
    textfield_->SetBackgroundEnabled(false);
    textfield_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(18, 20)));
    textfield_->GetViewAccessibility().SetName(u"Command bar");
    if (!new_tab_) {
      if (content::WebContents* contents = model_->GetActiveWebContents()) {
        textfield_->SetText(
            base::UTF8ToUTF16(contents->GetVisibleURL().spec()));
        textfield_->SelectAll(false);
      }
    }

    divider_ = AddChildView(std::make_unique<views::View>());
    divider_->SetPreferredSize(gfx::Size(0, 1));

    list_ = AddChildView(std::make_unique<views::BoxLayoutView>());
    list_->SetOrientation(views::BoxLayout::Orientation::kVertical);
    list_->SetInsideBorderInsets(gfx::Insets(6));
    list_->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);

    hint_ = AddChildView(std::make_unique<views::Label>(
        u"↑↓ move · ↵ open · esc close"));
    hint_->SetFontList(ArgonFont(kHintSize, gfx::Font::Weight::NORMAL));
    hint_->SetAutoColorReadabilityEnabled(false);
    hint_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    hint_->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(8, 20)));
  }
  CommandBarView(const CommandBarView&) = delete;
  CommandBarView& operator=(const CommandBarView&) = delete;
  ~CommandBarView() override = default;

  views::Textfield* textfield() { return textfield_; }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return gfx::Size(kWidth, GetLayoutManager()->GetPreferredHeightForWidth(
                                 this, kWidth));
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    // The panel's own background stands in for the frame.
    palette_ = ArgonPalette(
        GetColorProvider()->GetColor(ui::kColorDialogBackground));
    divider_->SetBackground(views::CreateSolidBackground(palette_.line));
    hint_->SetEnabledColor(palette_.muted);
    Update();
  }

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override {
    Update();
  }

  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::EventType::kKeyPressed) {
      return false;
    }
    switch (key_event.key_code()) {
      case ui::VKEY_DOWN:
        Select(selected_ + 1);
        return true;
      case ui::VKEY_UP:
        Select(selected_ - 1);
        return true;
      case ui::VKEY_RETURN:
        Run(selected_);
        return true;
      default:
        return false;
    }
  }

 private:
  // ponytail: tabs are matched by substring of title or URL, not ranked by
  // recency or fuzzy score; rank them if long tab lists make finding slow.
  void Update() {
    const std::u16string query(
        base::TrimWhitespace(textfield_->GetText(), base::TRIM_ALL));
    const std::u16string needle = base::i18n::ToLower(query);
    const auto matches = [&](std::u16string_view text) {
      return needle.empty() ||
             base::i18n::ToLower(text).find(needle) != std::u16string::npos;
    };
    results_.clear();

    if (new_tab_ && query.empty()) {
      results_.push_back(CommandResult{u"New tab", u"⌘T",
                          base::BindRepeating(&CommandBarView::OpenNewTabPage,
                                              base::Unretained(this))});
    }

    // Tabs in every space.
    int tab_count = 0;
    for (int i = 0; i < model_->count() && tab_count < kMaxTabs; ++i) {
      tabs::TabInterface* tab = model_->GetTabAtIndex(i);
      const std::u16string title = TabUIHelper::From(tab)->GetTitle();
      const std::u16string url =
          base::UTF8ToUTF16(tab->GetContents()->GetVisibleURL().spec());
      if (!matches(title) && !matches(url)) {
        continue;
      }
      std::u16string meta = u"Switch to tab";
      if (tab->IsPinned()) {
        meta = u"Pinned";
      } else if (const std::optional<tab_groups::TabGroupId> group =
                     tab->GetGroup()) {
        meta = SpaceName(model_, *group);
      }
      results_.push_back(CommandResult{title, meta,
                          base::BindRepeating(&CommandBarView::ActivateTab,
                                              base::Unretained(this),
                                              tab->GetWeakPtr())});
      ++tab_count;
    }

    // Open what was typed, or search for it, as the omnibox would.
    if (!query.empty()) {
      AutocompleteMatch match;
      AutocompleteClassifierFactory::GetForProfile(browser_->GetProfile())
          ->Classify(query, /*in_keyword_mode=*/false,
                     /*allow_exact_keyword_match=*/true,
                     metrics::OmniboxEventProto::OTHER, &match,
                     /*alternate_nav_url=*/nullptr);
      if (match.destination_url.is_valid()) {
        const bool search = AutocompleteMatch::IsSearchType(match.type);
        auto open = base::BindRepeating(&CommandBarView::Open,
                                        base::Unretained(this),
                                        match.destination_url,
                                        match.transition);
        // Typing a URL puts opening it first.
        results_.insert(
            search ? results_.end() : results_.begin(),
            CommandResult{search ? u"Search the web for “" + query + u"”"
                          : u"Open " + query,
                   new_tab_ ? u"New tab" : u"This tab", std::move(open)});
      }
    }

    // Commands: go to a space.
    if (!needle.empty() && model_->SupportsTabGroups() &&
        base::FeatureList::IsEnabled(features::kTabGroupsFocusing)) {
      for (const tab_groups::TabGroupId& space :
           model_->group_model()->ListTabGroups()) {
        const std::u16string name = SpaceName(model_, space);
        if (matches(name) || matches(u"go to " + name)) {
          results_.push_back(CommandResult{u"Go to " + name, u"Space",
                              base::BindRepeating(&CommandBarView::GoToSpace,
                                                  base::Unretained(this),
                                                  space)});
        }
      }
    }

    list_->RemoveAllChildViews();
    rows_.clear();
    for (size_t i = 0; i < results_.size(); ++i) {
      rows_.push_back(list_->AddChildView(std::make_unique<ResultRow>(
          results_[i], palette_,
          base::BindRepeating(&CommandBarView::Run, base::Unretained(this),
                              static_cast<int>(i)))));
    }
    list_->SetVisible(!rows_.empty());
    divider_->SetVisible(!rows_.empty());
    Select(0);
    PreferredSizeChanged();
  }

  void Select(int index) {
    if (rows_.empty()) {
      selected_ = 0;
      return;
    }
    selected_ = std::clamp(index, 0, static_cast<int>(rows_.size()) - 1);
    for (size_t i = 0; i < rows_.size(); ++i) {
      rows_[i]->SetSelected(static_cast<int>(i) == selected_);
    }
  }

  void Run(int index) {
    if (index < 0 || index >= static_cast<int>(results_.size())) {
      return;
    }
    // Close first: the action may move focus or replace this window's tab.
    // Widget::Close() is asynchronous, so `this` outlives the action.
    const base::RepeatingClosure run = results_[index].run;
    GetWidget()->Close();
    run.Run();
  }

  void OpenNewTabPage() {
    chrome::NewTab(browser_, NewTabTypes::kNewTabCommand);
  }

  void Open(const GURL& url, ui::PageTransition transition) {
    NavigateParams params(browser_, url, transition);
    params.disposition = new_tab_ ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                  : WindowOpenDisposition::CURRENT_TAB;
    Navigate(&params);
  }

  // Activating a tab in another space switches to it (argon_spaces.h).
  void ActivateTab(base::WeakPtr<tabs::TabInterface> tab) {
    if (!tab) {
      return;
    }
    const int index = model_->GetIndexOfTab(tab.get());
    if (index != TabStripModel::kNoTab) {
      model_->ActivateTabAt(index);
    }
  }

  void GoToSpace(const tab_groups::TabGroupId& space) {
    if (model_->group_model()->ContainsTabGroup(space)) {
      model_->EnterFocusMode(space);
    }
  }

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<TabStripModel> model_;
  const bool new_tab_;
  ArgonPalette palette_;

  raw_ptr<views::Textfield> textfield_ = nullptr;
  raw_ptr<views::View> divider_ = nullptr;
  raw_ptr<views::BoxLayoutView> list_ = nullptr;
  raw_ptr<views::Label> hint_ = nullptr;

  std::vector<CommandResult> results_;
  std::vector<raw_ptr<ResultRow>> rows_;
  int selected_ = 0;
};

BEGIN_METADATA(CommandBarView)
END_METADATA

// Owns the open command bar's delegate and widget, and deletes itself (and
// them) once the bar closes.
class CommandBar {
 public:
  CommandBar(BrowserWindowInterface* browser,
             views::Widget* parent,
             bool new_tab) {
    delegate_ = std::make_unique<views::BubbleDialogDelegate>(
        static_cast<views::View*>(nullptr), views::BubbleBorder::TOP_CENTER,
        views::BubbleBorder::DIALOG_SHADOW, /*autosize=*/true);
    // Centered over the window, kTopPercent from its top.
    const gfx::Rect window = parent->GetWindowBoundsInScreen();
    delegate_->SetAnchorRect(gfx::Rect(
        window.CenterPoint().x(),
        window.y() + window.height() * kTopPercent / 100, 0, 0));
    delegate_->set_parent_window(parent->GetNativeView());
    delegate_->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    delegate_->SetShowCloseButton(false);
    delegate_->SetShowTitle(false);
    delegate_->set_margins(gfx::Insets());
    delegate_->set_corner_radius(kCornerRadius);
    auto* view = delegate_->SetContentsView(
        std::make_unique<CommandBarView>(browser, new_tab));
    delegate_->SetInitiallyFocusedView(view->textfield());

    widget_ = views::BubbleDialogDelegate::CreateBubble(
        delegate_.get(),
        base::BindOnce(&CommandBar::OnClosed, base::Unretained(this)));
    widget_->Show();
  }
  CommandBar(const CommandBar&) = delete;
  CommandBar& operator=(const CommandBar&) = delete;
  ~CommandBar() = default;

  views::Widget* widget() { return widget_.get(); }

 private:
  void OnClosed(views::Widget::ClosedReason reason);

  // The delegate must outlive the widget, so it's declared (and built) first
  // and destroyed last.
  std::unique_ptr<views::BubbleDialogDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
};

// The open command bar, if any; one at a time across windows.
CommandBar* g_command_bar = nullptr;

void CommandBar::OnClosed(views::Widget::ClosedReason reason) {
  if (g_command_bar == this) {
    g_command_bar = nullptr;
  }
  // Not from inside the widget's own close notification.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
}

}  // namespace

bool ShowArgonCommandBar(BrowserWindowInterface* browser, bool new_tab) {
  if (browser->GetType() != BrowserWindowInterface::TYPE_NORMAL) {
    return false;
  }
  views::Widget* parent = views::Widget::GetWidgetForNativeWindow(
      browser->GetWindow()->GetNativeWindow());
  if (!parent || parent->IsMinimized()) {
    return false;
  }
  if (g_command_bar) {
    g_command_bar->widget()->Close();
    g_command_bar = nullptr;
  }
  g_command_bar = new CommandBar(browser, parent, new_tab);
  return true;
}

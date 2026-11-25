// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_toast.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/argon/argon_font.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/base_window.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/widget/widget.h"

namespace {

// From the handoff.
constexpr SkColor kBackground = SkColorSetRGB(0x1d, 0x1c, 0x1a);
constexpr SkColor kText = SkColorSetRGB(0xf3, 0xf1, 0xed);
constexpr SkColor kUndoBackground = SkColorSetARGB(0x24, 0xff, 0xff, 0xff);
constexpr int kCornerRadius = 12;
constexpr int kTextSize = 13;
constexpr base::TimeDelta kShowFor = base::Seconds(6);
// The toast's bottom edge, above the window's bottom.
constexpr int kBottomMargin = 28;

class Toast {
 public:
  Toast(views::Widget* parent, std::u16string text, base::OnceClosure undo)
      : undo_(std::move(undo)) {
    delegate_ = std::make_unique<views::BubbleDialogDelegate>(
        static_cast<views::View*>(nullptr), views::BubbleBorder::BOTTOM_CENTER,
        views::BubbleBorder::DIALOG_SHADOW, /*autosize=*/true);
    const gfx::Rect window = parent->GetWindowBoundsInScreen();
    delegate_->SetAnchorRect(gfx::Rect(window.CenterPoint().x(),
                                       window.bottom() - kBottomMargin, 0, 0));
    delegate_->set_parent_window(parent->GetNativeView());
    delegate_->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    delegate_->SetShowCloseButton(false);
    delegate_->SetShowTitle(false);
    delegate_->SetCanActivate(false);
    delegate_->set_close_on_deactivate(false);
    delegate_->set_margins(gfx::Insets());
    delegate_->set_corner_radius(kCornerRadius);
    delegate_->SetBackgroundColor(kBackground);

    auto row = std::make_unique<views::BoxLayoutView>();
    row->SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
    row->SetInsideBorderInsets(gfx::Insets::TLBR(8, 16, 8, 8));
    row->SetBetweenChildSpacing(12);
    auto* label = row->AddChildView(std::make_unique<views::Label>(text));
    label->SetFontList(ArgonFont(kTextSize, gfx::Font::Weight::NORMAL));
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(kText);
    if (undo_) {
      auto* undo_button =
          row->AddChildView(std::make_unique<views::LabelButton>(
              base::BindRepeating(&Toast::Undo, base::Unretained(this)),
              u"Undo"));
      undo_button->SetTextColor(views::Button::STATE_NORMAL, kText);
      undo_button->SetTextColor(views::Button::STATE_HOVERED, kText);
      undo_button->SetTextColor(views::Button::STATE_PRESSED, kText);
      undo_button->SetBackground(
          views::CreateRoundedRectBackground(kUndoBackground, 8));
      undo_button->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(4, 10)));
    } else {
      row->SetInsideBorderInsets(gfx::Insets::VH(10, 16));
    }
    delegate_->SetContentsView(std::move(row));

    widget_ = views::BubbleDialogDelegate::CreateBubble(
        delegate_.get(), base::BindOnce(&Toast::OnClosed, base::Unretained(this)));
    widget_->ShowInactive();
    timer_.Start(FROM_HERE, kShowFor,
                 base::BindOnce(&views::Widget::Close,
                                base::Unretained(widget_.get())));
  }
  Toast(const Toast&) = delete;
  Toast& operator=(const Toast&) = delete;
  ~Toast() = default;

  views::Widget* widget() { return widget_.get(); }

 private:
  void Undo() {
    timer_.Stop();
    widget_->Close();
    if (undo_) {
      std::move(undo_).Run();
    }
  }

  void OnClosed(views::Widget::ClosedReason reason);

  base::OnceClosure undo_;
  base::OneShotTimer timer_;
  // The delegate must outlive the widget: declared first, destroyed last.
  std::unique_ptr<views::BubbleDialogDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
};

Toast* g_toast = nullptr;

void Toast::OnClosed(views::Widget::ClosedReason reason) {
  timer_.Stop();
  if (g_toast == this) {
    g_toast = nullptr;
  }
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE, this);
}

}  // namespace

void ShowArgonToast(BrowserWindowInterface* browser,
                    std::u16string text,
                    base::OnceClosure undo) {
  views::Widget* parent = views::Widget::GetWidgetForNativeWindow(
      browser->GetWindow()->GetNativeWindow());
  if (!parent || parent->IsMinimized()) {
    return;
  }
  if (g_toast) {
    g_toast->widget()->Close();
    g_toast = nullptr;
  }
  g_toast = new Toast(parent, std::move(text), std::move(undo));
}

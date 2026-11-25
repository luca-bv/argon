// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#include "chrome/browser/ui/views/argon/argon_edge_pill.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/views/argon/argon_font.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"

namespace {

// From the handoff: 4px dots, the current one 14px tall; 34px buttons in a
// pill padded by 5px.
constexpr int kDot = 4;
constexpr int kCurrentDot = 14;
constexpr int kDotGap = 4;
constexpr int kButton = 34;
constexpr int kButtonGap = 2;
constexpr int kPadding = 5;
constexpr int kButtonTextSize = 13;

// LabelButton keeps label() protected; the font is set through it.
class PillButton : public views::LabelButton {
 public:
  using LabelButton::LabelButton;
  using LabelButton::label;
};

}  // namespace

ArgonEdgePill::ArgonEdgePill(base::RepeatingCallback<void(size_t)> on_switch)
    : on_switch_(std::move(on_switch)) {
  // Over the sidebar's rows while expanded.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetNotifyEnterExitOnChild(true);
  GetViewAccessibility().SetRole(ax::mojom::Role::kGroup);
  GetViewAccessibility().SetName(u"Spaces");
}

ArgonEdgePill::~ArgonEdgePill() = default;

void ArgonEdgePill::SetSpaces(std::vector<Space> spaces,
                              const ArgonPalette& palette) {
  spaces_ = std::move(spaces);
  palette_ = palette;
  if (expanded_) {
    RebuildButtons();
  }
  PreferredSizeChanged();
  SchedulePaint();
}

gfx::Size ArgonEdgePill::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  if (!expanded_) {
    return gfx::Size(kRestWidth, kRestHeight);
  }
  const int count = std::max(static_cast<int>(spaces_.size()), 1);
  const int button = ButtonSize();
  return gfx::Size(button + 2 * kPadding,
                   count * button + (count - 1) * kButtonGap + 2 * kPadding);
}

// kButton, or less where the sidebar is narrower (collapsed).
int ArgonEdgePill::ButtonSize() const {
  if (!parent() || parent()->width() == 0) {
    return kButton;
  }
  return std::clamp(parent()->width() - 2 * kPadding, 16, kButton);
}

void ArgonEdgePill::OnPaint(gfx::Canvas* canvas) {
  const gfx::RectF bounds(GetLocalBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  if (expanded_) {
    // A raised pill in the page color, with a hairline.
    flags.setColor(GetColorProvider()->GetColor(ui::kColorDialogBackground));
    canvas->DrawRoundRect(bounds, bounds.width() / 2, flags);
    flags.setColor(palette_.line);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    gfx::RectF ring = bounds;
    ring.Inset(0.5f);
    canvas->DrawRoundRect(ring, ring.width() / 2, flags);
    return;
  }
  flags.setColor(palette_.line);
  canvas->DrawRoundRect(bounds, bounds.width() / 2, flags);
  // One dot per space, centered; the current space's is taller and accented.
  int total = 0;
  for (const Space& space : spaces_) {
    total += (space.current ? kCurrentDot : kDot) + kDotGap;
  }
  total -= kDotGap;
  float y = (bounds.height() - std::min<float>(total, bounds.height())) / 2;
  const float x = (bounds.width() - kDot) / 2;
  for (const Space& space : spaces_) {
    const int height = space.current ? kCurrentDot : kDot;
    if (y + height > bounds.height()) {
      break;  // More spaces than fit at rest; the expanded pill has them all.
    }
    flags.setColor(space.current ? space.accent : palette_.muted);
    canvas->DrawRoundRect(gfx::RectF(x, y, kDot, height), kDot / 2.0f, flags);
    y += height + kDotGap;
  }
}

void ArgonEdgePill::OnMouseEntered(const ui::MouseEvent& event) {
  SetExpanded(true);
}

void ArgonEdgePill::OnMouseExited(const ui::MouseEvent& event) {
  SetExpanded(false);
}

void ArgonEdgePill::SetExpanded(bool expanded) {
  if (expanded == expanded_) {
    return;
  }
  expanded_ = expanded;
  if (expanded_) {
    RebuildButtons();
  } else {
    RemoveAllChildViews();
  }
  PreferredSizeChanged();
  if (parent()) {
    parent()->InvalidateLayout();
  }
  SchedulePaint();
}

void ArgonEdgePill::RebuildButtons() {
  RemoveAllChildViews();
  for (size_t i = 0; i < spaces_.size(); ++i) {
    const Space& space = spaces_[i];
    auto* button = AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(on_switch_, i), space.name.substr(0, 1)));
    const int size = ButtonSize();
    button->SetBounds(kPadding,
                      kPadding + static_cast<int>(i) * (size + kButtonGap),
                      size, size);
    button->SetHorizontalAlignment(gfx::ALIGN_CENTER);
    button->SetTooltipText(space.name);
    button->GetViewAccessibility().SetName(space.name);
    button->label()->SetFontList(
        ArgonFont(kButtonTextSize, gfx::Font::Weight::SEMIBOLD));
    button->label()->SetAutoColorReadabilityEnabled(false);
    const SkColor text = space.current ? SK_ColorWHITE : space.accent;
    for (const auto state : {views::Button::STATE_NORMAL,
                             views::Button::STATE_HOVERED,
                             views::Button::STATE_PRESSED}) {
      button->SetTextColor(state, text);
    }
    button->SetBackground(views::CreateRoundedRectBackground(
        space.current ? space.accent : palette_.hover, ButtonSize() / 2));
  }
}

BEGIN_METADATA(ArgonEdgePill)
END_METADATA

// Copyright 2026 The Argon Authors
// Licensed under the GNU General Public License v3.0; see Argon's LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_EDGE_PILL_H_
#define CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_EDGE_PILL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "chrome/browser/ui/views/argon/argon_palette.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

// The edge pill (design_handoff_argon_browser/README.md, "Edge pill"): on the
// sidebar's leading edge, vertically centered. At rest a thin pill with a dot
// per space, the current one taller and in its accent; hovered, it expands
// into a button per space. It's the space switcher while the sidebar is
// collapsed, when the switcher row isn't drawn.
class ArgonEdgePill : public views::View {
  METADATA_HEADER(ArgonEdgePill, views::View)

 public:
  struct Space {
    std::u16string name;
    SkColor accent;
    bool current = false;
  };
  // At rest: kRestWidth wide, kRestHeight tall.
  static constexpr int kRestWidth = 6;
  static constexpr int kRestHeight = 56;

  explicit ArgonEdgePill(base::RepeatingCallback<void(size_t)> on_switch);
  ArgonEdgePill(const ArgonEdgePill&) = delete;
  ArgonEdgePill& operator=(const ArgonEdgePill&) = delete;
  ~ArgonEdgePill() override;

  void SetSpaces(std::vector<Space> spaces, const ArgonPalette& palette);
  bool expanded() const { return expanded_; }

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

 private:
  void SetExpanded(bool expanded);
  int ButtonSize() const;
  void RebuildButtons();

  const base::RepeatingCallback<void(size_t)> on_switch_;
  std::vector<Space> spaces_;
  ArgonPalette palette_;
  bool expanded_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_ARGON_ARGON_EDGE_PILL_H_

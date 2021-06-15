// Copyright (c) 2021 Ryan Gonzalez.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_UI_VIEWS_CLIENT_FRAME_VIEW_LINUX_H_
#define SHELL_BROWSER_UI_VIEWS_CLIENT_FRAME_VIEW_LINUX_H_

#include <array>
#include <memory>
#include <vector>

#include "shell/browser/ui/views/frameless_view.h"
#include "ui/native_theme/native_theme_observer.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/linux_ui/nav_button_provider.h"
#include "ui/views/linux_ui/window_button_order_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_buttons.h"

namespace electron {

class ClientFrameViewLinux : public FramelessView,
                             public ui::NativeThemeObserver,
                             public views::WindowButtonOrderObserver {
 public:
  static const char kViewClassName[];
  ClientFrameViewLinux();
  ~ClientFrameViewLinux() override;

  void Init(NativeWindowViews* window, views::Widget* frame) override;

 protected:
  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // views::WindowButtonOrderObserver:
  void OnWindowButtonOrderingChange(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons) override;

  // Overriden from FramelessView:
  int ResizingBorderHitTest(const gfx::Point& point) override;

  // Overriden from views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // Overridden from View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void Layout() override;
  void OnPaint(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 private:
  static constexpr int kNavButtonCount = 4;

  struct NavButton {
    views::NavButtonProvider::FrameButtonDisplayType type;
    views::FrameButton frame_button;
    void (views::Widget::*callback)();
    int accessibility_id;
    int hit_test_id;
    views::ImageButton* button{nullptr};
  };

  struct ThemeValues {
    int window_border_radius;

    int titlebar_min_height;
    gfx::Insets titlebar_padding;

    SkColor title_color;
    gfx::Insets title_padding;

    int button_min_size;
    gfx::Insets button_padding;
  };

  void PaintAsActiveChanged();

  void UpdateThemeValues();

  void PropagateShadowInsets();

  enum class ButtonSide { kLeading, kTrailing };

  views::NavButtonProvider::FrameButtonDisplayType GetButtonTypeToSkip() const;
  void UpdateButtonImages();
  void LayoutButtons(gfx::Rect* remaining_content_bounds);
  void LayoutButtonsOnSide(ButtonSide side,
                           gfx::Rect* remaining_content_bounds);

  void PaintShadow(gfx::Canvas* canvas);
  void PaintBorder(gfx::Canvas* canvas);
  void PaintTitlebar(gfx::Canvas* canvas);

  gfx::Insets GetBorderDecorationInsets() const;

  gfx::Rect GetWindowContentBounds() const;
  SkRRect GetRoundedRectForBounds(gfx::RectF bounds) const;

  gfx::Rect GetTitlebarBounds() const;
  gfx::Insets GetTitlebarContentInsets() const;
  gfx::Rect GetTitlebarContentBounds() const;

  gfx::Size SizeWithDecorations(gfx::Size size) const;

  base::CallbackListSubscription paint_as_active_changed_subscription_;

  ThemeValues theme_values_;

  views::Label* title_;

  std::unique_ptr<views::NavButtonProvider> nav_button_provider_;
  std::array<NavButton, kNavButtonCount> nav_buttons_;

  std::vector<views::FrameButton> leading_frame_buttons_;
  std::vector<views::FrameButton> trailing_frame_buttons_;
};

}  // namespace electron

#endif  // SHELL_BROWSER_UI_VIEWS_CLIENT_FRAME_VIEW_LINUX_H_

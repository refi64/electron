// Copyright (c) 2021 Ryan Gonzalez.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/ui/views/client_frame_view_linux.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkColor.h"
#include "shell/browser/native_window_views.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gtk/gtk_compat.h"  // nogncheck
#include "ui/gtk/gtk_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/linux_ui/nav_button_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/frame_buttons.h"

#if defined(USE_OZONE)
#include "ui/ozone/platform/wayland/host/shell_toplevel_wrapper.h"  // nogncheck
#include "ui/ozone/platform/wayland/host/wayland_toplevel_window.h"  // nogncheck
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"
#endif

namespace electron {

namespace {

// We can't read border-radius-top-[left/right], so just hardcode
// Adwaita's value for now.
constexpr int kAdwaitaBorderRadius = 8;

// Also hardcode Adwaita's box-shadow.
constexpr int kAdwaitaShadowXOffset = 0;
constexpr int kAdwaitaShadowYOffset = 3;

constexpr int kAdwaitaShadowBlur = 9;
// Skia's sigma value used for shadow blur is the CSS box-shadow / 2.
constexpr float kAdwaitaShadowSigma = kAdwaitaShadowBlur / 2.0f;

constexpr SkColor kAdwaitaShadowColor = SkColorSetA(SK_ColorBLACK, 255 / 2);

// Note that the code assumes the border inset is immediately inside the shadow
// inset!

// As per the Chromium source code, the sigma * 3 should be enough to display
// the full shadow.
constexpr int kShadowInset = kAdwaitaShadowSigma * 3;
constexpr int kBorderInset = 1;

constexpr int kTotalBorderDecorationsInset = kShadowInset + kBorderInset;

}  // namespace

// static
const char ClientFrameViewLinux::kViewClassName[] = "ClientFrameView";

ClientFrameViewLinux::ClientFrameViewLinux()
    : nav_button_provider_(
          views::LinuxUI::instance()->CreateNavButtonProvider()),
      nav_buttons_{
          NavButton{views::NavButtonProvider::FrameButtonDisplayType::kClose,
                    views::FrameButton::kClose, &views::Widget::Close,
                    IDS_APP_ACCNAME_CLOSE, HTCLOSE},
          NavButton{views::NavButtonProvider::FrameButtonDisplayType::kMaximize,
                    views::FrameButton::kMaximize, &views::Widget::Maximize,
                    IDS_APP_ACCNAME_MAXIMIZE, HTMAXBUTTON},
          NavButton{views::NavButtonProvider::FrameButtonDisplayType::kRestore,
                    views::FrameButton::kMaximize, &views::Widget::Restore,
                    IDS_APP_ACCNAME_RESTORE, HTMAXBUTTON},
          NavButton{views::NavButtonProvider::FrameButtonDisplayType::kMinimize,
                    views::FrameButton::kMinimize, &views::Widget::Minimize,
                    IDS_APP_ACCNAME_MINIMIZE, HTMINBUTTON},
      },
      trailing_frame_buttons_{views::FrameButton::kMinimize,
                              views::FrameButton::kMaximize,
                              views::FrameButton::kClose} {
  for (auto& button : nav_buttons_) {
    button.button = new views::ImageButton();
    button.button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    button.button->SetAccessibleName(
        l10n_util::GetStringUTF16(button.accessibility_id));
  }

  title_ = new views::Label();
  title_->SetSubpixelRenderingEnabled(false);
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_->SetVerticalAlignment(gfx::ALIGN_MIDDLE);
  title_->SetTextStyle(views::style::STYLE_TAB_ACTIVE);
  AddChildView(title_);

  for (NavButton& button : nav_buttons_) {
    AddChildView(button.button);
  }

  GetNativeTheme()->AddObserver(this);
  views::LinuxUI::instance()->AddWindowButtonOrderObserver(this);
}

ClientFrameViewLinux::~ClientFrameViewLinux() {
  views::LinuxUI::instance()->RemoveWindowButtonOrderObserver(this);
  GetNativeTheme()->RemoveObserver(this);
}

void ClientFrameViewLinux::Init(NativeWindowViews* window,
                                views::Widget* frame) {
  FramelessView::Init(window, frame);

  paint_as_active_changed_subscription_ =
      frame_->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &ClientFrameViewLinux::PaintAsActiveChanged, base::Unretained(this)));

  UpdateWindowTitle();

  for (auto& button : nav_buttons_) {
    button.button->SetCallback(
        base::BindRepeating(button.callback, base::Unretained(frame)));
  }

  UpdateThemeValues();
}

void ClientFrameViewLinux::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  UpdateThemeValues();
}

void ClientFrameViewLinux::OnWindowButtonOrderingChange(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  leading_frame_buttons_ = leading_buttons;
  trailing_frame_buttons_ = trailing_buttons;

  InvalidateLayout();
}

int ClientFrameViewLinux::ResizingBorderHitTest(const gfx::Point& point) {
  return ResizingBorderHitTestImpl(point, kTotalBorderDecorationsInset);
}

gfx::Rect ClientFrameViewLinux::GetBoundsForClientView() const {
  gfx::Rect client_bounds = bounds();
  client_bounds.Inset(GetBorderDecorationInsets());
  client_bounds.Inset(0, GetTitlebarBounds().height(), 0, 0);
  return client_bounds;
}

gfx::Rect ClientFrameViewLinux::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Insets insets = bounds().InsetsFrom(GetBoundsForClientView());
  return gfx::Rect(std::max(0, client_bounds.x() - insets.left()),
                   std::max(0, client_bounds.y() - insets.top()),
                   client_bounds.width() + insets.width(),
                   client_bounds.height() + insets.height());
}

int ClientFrameViewLinux::NonClientHitTest(const gfx::Point& point) {
  for (auto& button : nav_buttons_) {
    if (button.button->GetVisible() &&
        button.button->GetMirroredBounds().Contains(point)) {
      return button.hit_test_id;
    }
  }

  if (GetTitlebarBounds().Contains(point)) {
    return HTCAPTION;
  }

  return FramelessView::NonClientHitTest(point);
}

void ClientFrameViewLinux::UpdateWindowTitle() {
  title_->SetText(base::UTF8ToUTF16(window_->GetTitle()));
}

void ClientFrameViewLinux::SizeConstraintsChanged() {
  InvalidateLayout();
}

gfx::Size ClientFrameViewLinux::CalculatePreferredSize() const {
  return SizeWithDecorations(FramelessView::CalculatePreferredSize());
}

gfx::Size ClientFrameViewLinux::GetMinimumSize() const {
  return SizeWithDecorations(FramelessView::GetMinimumSize());
}

gfx::Size ClientFrameViewLinux::GetMaximumSize() const {
  return SizeWithDecorations(FramelessView::GetMaximumSize());
}

void ClientFrameViewLinux::Layout() {
  FramelessView::Layout();

  PropagateShadowInsets();

  if (frame_->IsFullscreen()) {
    // Just hide everything and return.
    for (NavButton& button : nav_buttons_) {
      button.button->SetVisible(false);
    }

    title_->SetVisible(false);
    return;
  }

  UpdateButtonImages();

  gfx::Rect remaining_content_bounds = GetTitlebarContentBounds();
  LayoutButtons(&remaining_content_bounds);

  gfx::Rect title_bounds(remaining_content_bounds);
  title_bounds.Inset(theme_values_.title_padding);

  title_->SetVisible(true);
  title_->SetBounds(title_bounds.x(), title_bounds.y(), title_bounds.width(),
                    title_bounds.height());
}

void ClientFrameViewLinux::OnPaint(gfx::Canvas* canvas) {
  PaintShadow(canvas);
  PaintBorder(canvas);

  if (!frame_->IsFullscreen()) {
    PaintTitlebar(canvas);
  }
}

const char* ClientFrameViewLinux::GetClassName() const {
  return kViewClassName;
}

void ClientFrameViewLinux::PaintAsActiveChanged() {
  UpdateThemeValues();
}

void ClientFrameViewLinux::UpdateThemeValues() {
  gtk::GtkCssContext window_context =
      gtk::AppendCssNodeToStyleContext({}, "GtkWindow#window.background.csd");
  gtk::GtkCssContext headerbar_context = gtk::AppendCssNodeToStyleContext(
      {}, "GtkHeaderBar#headerbar.default-decoration.titlebar");
  gtk::GtkCssContext title_context = gtk::AppendCssNodeToStyleContext(
      headerbar_context, "GtkLabel#label.title");
  gtk::GtkCssContext button_context = gtk::AppendCssNodeToStyleContext(
      headerbar_context, "GtkButton#button.image-button");

  gtk_style_context_set_parent(headerbar_context, window_context);
  gtk_style_context_set_parent(title_context, headerbar_context);
  gtk_style_context_set_parent(button_context, headerbar_context);

  // ShouldPaintAsActive asks the widget, so assume active if the widget is not
  // set yet.
  if (GetWidget() != nullptr && !ShouldPaintAsActive()) {
    gtk_style_context_set_state(window_context, GTK_STATE_FLAG_BACKDROP);
    gtk_style_context_set_state(headerbar_context, GTK_STATE_FLAG_BACKDROP);
    gtk_style_context_set_state(title_context, GTK_STATE_FLAG_BACKDROP);
    gtk_style_context_set_state(button_context, GTK_STATE_FLAG_BACKDROP);
  }

  // Hardcoded due to the underlying properties being seemingly inaccessible.
  theme_values_.window_border_radius = kAdwaitaBorderRadius;

  gtk::GtkStyleContextGet(headerbar_context, "min-height",
                          &theme_values_.titlebar_min_height, nullptr);
  theme_values_.titlebar_padding =
      gtk::GtkStyleContextGetPadding(headerbar_context);

  theme_values_.title_color = gtk::GtkStyleContextGetColor(title_context);
  theme_values_.title_padding = gtk::GtkStyleContextGetPadding(title_context);

  gtk::GtkStyleContextGet(button_context, "min-height",
                          &theme_values_.button_min_size, nullptr);
  theme_values_.button_padding = gtk::GtkStyleContextGetPadding(button_context);

  title_->SetEnabledColor(theme_values_.title_color);

  InvalidateLayout();
  SchedulePaint();
}

void ClientFrameViewLinux::PropagateShadowInsets() {
#if defined(USE_OZONE)
  // XXX: This is ugly, but we need to propagate our shadow insets to the
  // Wayland compositor, and this is the easiest place to do it. We can assume
  // given the current implementation of Ozone that Wayland is the only Linux
  // platform that has no title bar, and Electron only uses top-level windows,
  // thus it should be safe to use these casts.
  // Also, the reason the + 1 is necessary in the insets calculation is unknown.

  views::DesktopWindowTreeHostPlatform* tree_host =
      views::DesktopWindowTreeHostLinux::GetHostForWidget(
          window_->GetAcceleratedWidget());
  auto* toplevel_window =
      static_cast<ui::WaylandToplevelWindow*>(tree_host->platform_window());
  toplevel_window->UpdateContentInsets(
      gfx::Insets(kTotalBorderDecorationsInset + 1));
#endif
}

views::NavButtonProvider::FrameButtonDisplayType
ClientFrameViewLinux::GetButtonTypeToSkip() const {
  return frame_->IsMaximized()
             ? views::NavButtonProvider::FrameButtonDisplayType::kMaximize
             : views::NavButtonProvider::FrameButtonDisplayType::kRestore;
}

void ClientFrameViewLinux::UpdateButtonImages() {
  nav_button_provider_->RedrawImages(theme_values_.button_min_size,
                                     frame_->IsMaximized(),
                                     ShouldPaintAsActive());

  views::NavButtonProvider::FrameButtonDisplayType skip_type =
      GetButtonTypeToSkip();

  for (NavButton& button : nav_buttons_) {
    if (button.type == skip_type) {
      continue;
    }

    for (size_t state_id = 0; state_id < views::Button::STATE_COUNT;
         state_id++) {
      views::Button::ButtonState state =
          static_cast<views::Button::ButtonState>(state_id);
      button.button->SetImage(
          state, nav_button_provider_->GetImage(button.type, state));
    }
  }
}

void ClientFrameViewLinux::LayoutButtons(gfx::Rect* remaining_content_bounds) {
  for (NavButton& button : nav_buttons_) {
    button.button->SetVisible(false);
  }

  LayoutButtonsOnSide(ButtonSide::kLeading, remaining_content_bounds);
  LayoutButtonsOnSide(ButtonSide::kTrailing, remaining_content_bounds);
}

void ClientFrameViewLinux::LayoutButtonsOnSide(
    ButtonSide side,
    gfx::Rect* remaining_content_bounds) {
  views::NavButtonProvider::FrameButtonDisplayType skip_type =
      GetButtonTypeToSkip();

  std::vector<views::FrameButton> frame_buttons;

  switch (side) {
    case ButtonSide::kLeading:
      frame_buttons = leading_frame_buttons_;
      break;
    case ButtonSide::kTrailing:
      frame_buttons = trailing_frame_buttons_;
      // We always lay buttons out going from the edge towards the center, but
      // they are given to us as left-to-right, so reverse them.
      std::reverse(frame_buttons.begin(), frame_buttons.end());
      break;
    default:
      NOTREACHED();
  }

  for (views::FrameButton frame_button : frame_buttons) {
    auto* button = std::find_if(
        nav_buttons_.begin(), nav_buttons_.end(), [&](const NavButton& test) {
          return test.type != skip_type && test.frame_button == frame_button;
        });
    CHECK(button != nav_buttons_.end())
        << "Failed to find frame button: " << static_cast<int>(frame_button);

    if (button->type == skip_type) {
      continue;
    }

    button->button->SetVisible(true);

    int button_width = theme_values_.button_min_size;
    int next_button_offset =
        button_width + nav_button_provider_->GetInterNavButtonSpacing();

    int x_position = 0;
    gfx::Insets inset_after_placement;

    switch (side) {
      case ButtonSide::kLeading:
        x_position = remaining_content_bounds->x();
        inset_after_placement.set_left(next_button_offset);
        break;
      case ButtonSide::kTrailing:
        x_position = remaining_content_bounds->right() - button_width;
        inset_after_placement.set_right(next_button_offset);
        break;
      default:
        NOTREACHED();
    }

    button->button->SetBounds(x_position, remaining_content_bounds->y(),
                              button_width, remaining_content_bounds->height());
    remaining_content_bounds->Inset(inset_after_placement);
  }
}

void ClientFrameViewLinux::PaintShadow(gfx::Canvas* canvas) {
  gfx::RectF shadow(bounds());
  shadow.Inset(kShadowInset, kShadowInset);

  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setImageFilter(sk_make_sp<cc::DropShadowPaintFilter>(
      kAdwaitaShadowXOffset, kAdwaitaShadowYOffset, kAdwaitaShadowSigma,
      kAdwaitaShadowSigma, kAdwaitaShadowColor,
      cc::DropShadowPaintFilter::ShadowMode::kDrawShadowOnly, nullptr));
  canvas->sk_canvas()->drawRRect(GetRoundedRectForBounds(shadow), flags);
}

void ClientFrameViewLinux::PaintBorder(gfx::Canvas* canvas) {
  // Note that View has integrated border functionality. However, as we already
  // have to manage our own insets for the shadows, and the built-in border
  // tools do not support borders on only the top of the window, it's easier to
  // just draw the entire border ourselves.

  gfx::RectF border(bounds());
  border.Inset(kShadowInset, kShadowInset);
  border.Inset(kBorderInset / 2.0f, kBorderInset / 2.0f);

  cc::PaintFlags flags;
  // For unknown reasons, the border tends to be drawn too far outwards at the
  // corners, resulting in a gap. Compensate for this by doubling the border's
  // size, then any parts that bleed into the titlebar region will get painted
  // over afterwards anyway.
  flags.setStrokeWidth(kBorderInset * 2);
  flags.setColor(SK_ColorBLACK);
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);

  canvas->sk_canvas()->drawRRect(GetRoundedRectForBounds(border), flags);
}

void ClientFrameViewLinux::PaintTitlebar(gfx::Canvas* canvas) {
  gfx::RectF content_bounds(GetWindowContentBounds());

  canvas->Save();
  canvas->ClipPath(SkPath::RRect(GetRoundedRectForBounds(content_bounds)),
                   true);

  ui::NativeTheme::ExtraParams params;
  params.frame_top_area.use_custom_frame = true;
  params.frame_top_area.is_active = ShouldPaintAsActive();
  GetNativeTheme()->Paint(
      canvas->sk_canvas(), ui::NativeTheme::Part::kFrameTopArea,
      ui::NativeTheme::kNormal, GetTitlebarBounds(), params);

  canvas->Restore();
}

gfx::Insets ClientFrameViewLinux::GetBorderDecorationInsets() const {
  return gfx::Insets(kTotalBorderDecorationsInset);
}

gfx::Rect ClientFrameViewLinux::GetWindowContentBounds() const {
  gfx::Rect content_bounds = bounds();
  content_bounds.Inset(GetBorderDecorationInsets());
  return content_bounds;
}

SkRRect ClientFrameViewLinux::GetRoundedRectForBounds(gfx::RectF bounds) const {
  SkRect rect = gfx::RectFToSkRect(bounds);
  SkRRect rrect;

  if (!frame_->IsMaximized()) {
    SkPoint round_point{theme_values_.window_border_radius,
                        theme_values_.window_border_radius};
    SkPoint radii[] = {round_point, round_point, {}, {}};
    rrect.setRectRadii(rect, radii);
  } else {
    rrect.setRect(rect);
  }

  return rrect;
}

gfx::Rect ClientFrameViewLinux::GetTitlebarBounds() const {
  if (frame_->IsFullscreen()) {
    return gfx::Rect();
  }

  int font_height = gfx::FontList().GetHeight();
  int titlebar_height =
      std::max(font_height, theme_values_.titlebar_min_height) +
      GetTitlebarContentInsets().height();

  gfx::Insets decoration_insets = GetBorderDecorationInsets();

  gfx::Rect titlebar(width(), titlebar_height + decoration_insets.height());
  titlebar.Inset(decoration_insets);
  return titlebar;
}

gfx::Insets ClientFrameViewLinux::GetTitlebarContentInsets() const {
  return theme_values_.titlebar_padding +
         nav_button_provider_->GetTopAreaSpacing();
}

gfx::Rect ClientFrameViewLinux::GetTitlebarContentBounds() const {
  gfx::Rect titlebar(GetTitlebarBounds());
  titlebar.Inset(GetTitlebarContentInsets());
  return titlebar;
}

gfx::Size ClientFrameViewLinux::SizeWithDecorations(gfx::Size size) const {
  gfx::Insets decoration_insets = GetBorderDecorationInsets();

  size.Enlarge(0, GetTitlebarBounds().height());
  size.Enlarge(decoration_insets.width(), decoration_insets.height());
  return size;
}

}  // namespace electron

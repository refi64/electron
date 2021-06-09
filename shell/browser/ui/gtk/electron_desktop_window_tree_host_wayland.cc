// Copyright (c) 2021 Ryan Gonzalez.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "shell/browser/ui/gtk/electron_desktop_window_tree_host_wayland.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace electron {

ElectronDesktopWindowTreeHostWayland::ElectronDesktopWindowTreeHostWayland(
    NativeWindowViews* native_window_view,
    views::DesktopNativeWidgetAura* desktop_native_widget_aura)
    : views::DesktopWindowTreeHostLinux(native_window_view->widget(),
                                        desktop_native_widget_aura),
      native_window_view_(native_window_view) {}

ElectronDesktopWindowTreeHostWayland::~ElectronDesktopWindowTreeHostWayland() =
    default;

bool ElectronDesktopWindowTreeHostWayland::ShouldWindowContentsBeTransparent()
    const {
  return !native_window_view_->IsFullscreen() ||
         native_window_view_->transparent();
}

}  // namespace electron

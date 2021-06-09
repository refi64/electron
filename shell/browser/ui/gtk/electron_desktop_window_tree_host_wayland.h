// Copyright (c) 2021 Ryan Gonzalez.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef SHELL_BROWSER_UI_GTK_ELECTRON_DESKTOP_WINDOW_TREE_HOST_WAYLAND_H_
#define SHELL_BROWSER_UI_GTK_ELECTRON_DESKTOP_WINDOW_TREE_HOST_WAYLAND_H_

#include <windows.h>

#include "shell/browser/native_window_views.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace electron {

class ElectronDesktopWindowTreeHostWayland
    : public views::DesktopWindowTreeHostLinux {
 public:
  ElectronDesktopWindowTreeHostWayland(
      NativeWindowViews* native_window_view,
      views::DesktopNativeWidgetAura* desktop_native_widget_aura);
  ~ElectronDesktopWindowTreeHostWayland() override;

 protected:
  // Overridden from DesktopWindowTreeHost:
  bool ShouldWindowContentsBeTransparent() const override;

 private:
  NativeWindowViews* native_window_view_;
};

}  // namespace electron

#endif  // SHELL_BROWSER_UI_GTK_ELECTRON_DESKTOP_WINDOW_TREE_HOST_WAYLAND_H_

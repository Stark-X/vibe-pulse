#pragma once
#include "WindowManager.h"

// WindowManager implementation backed by Hyprland IPC (hyprctl).
// Requires HYPRLAND_INSTANCE_SIGNATURE to be set in the environment.
class HyprlandWindowManager final : public WindowManager {
public:
    QString findWindowByPid(quint32 pid) const override;
    void    focusWindow(const QString &windowId) override;
    // Queries the window's current position before resizing so that
    // resizewindowpixel (which scales from center) doesn't drift the overlay.
    void    resizeWindow(const QString &windowId, int w, int h) override;
    bool    needsExternalResize() const override { return true; }
};

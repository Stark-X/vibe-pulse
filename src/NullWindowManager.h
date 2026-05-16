#pragma once
#include "WindowManager.h"

// No-op WindowManager for compositors without Hyprland-specific IPC.
// "find window" and "focus" are unavailable; layer-shell set_size handles resize.
class NullWindowManager final : public WindowManager {
public:
    QString findWindowByPid(quint32) const override { return {}; }
    void    focusWindow(const QString &) override {}
    void    resizeWindow(const QString &, int, int) override {}
    bool    needsExternalResize() const override { return false; }
};

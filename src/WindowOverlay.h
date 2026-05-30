#pragma once
#include <memory>
#include <QWindow>

struct OverlayOptions {
    enum Role { MainCard, NotchLeftHud, NotchRightHud };
    Role role = MainCard;
    bool ignoresMouseEvents = false;
};

// Platform-specific floating overlay setup.
// Implementations: WindowOverlay_linux.cpp / _macos.mm / _win.cpp
class WindowOverlay {
public:
    virtual ~WindowOverlay() = default;

    // Called once after QWindow is shown; platform sets level/layer/topmost.
    virtual void setup(QWindow *win, const OverlayOptions &opts = {}) = 0;

    // Force a notch HUD window to (x, y) in Qt global coordinates.
    // Default falls back to QWindow::setPosition; macOS overrides with direct
    // NSWindow frame call to bypass menu-bar safe-area clamping.
    virtual void placeNotchHud(QWindow *win, int x, int y) {
        win->setPosition(x, y);
    }

    static std::unique_ptr<WindowOverlay> create();
};

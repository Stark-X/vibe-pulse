#pragma once
#include <memory>
class QWindow;

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

    static std::unique_ptr<WindowOverlay> create();
};

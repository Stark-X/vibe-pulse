#pragma once
#include <memory>
class QWindow;

// Platform-specific floating overlay setup.
// Implementations: WindowOverlay_linux.cpp / _macos.mm / _win.cpp
class WindowOverlay {
public:
    virtual ~WindowOverlay() = default;

    // Called once after QWindow is shown; platform sets level/layer/topmost.
    virtual void setup(QWindow *win) = 0;

    static std::unique_ptr<WindowOverlay> create();
};

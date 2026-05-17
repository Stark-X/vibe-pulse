#include "WindowOverlay.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

class WindowsWindowOverlay final : public WindowOverlay {
public:
    void setup(QWindow *win, const OverlayOptions &) override {
        HWND hwnd = reinterpret_cast<HWND>(win->winId());

        // Always-on-top, no taskbar button
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        LONG ex = GetWindowLong(hwnd, GWL_EXSTYLE);
        SetWindowLong(hwnd, GWL_EXSTYLE,
                      (ex | WS_EX_TOOLWINDOW) & ~WS_EX_APPWINDOW);

        // Position top-right of primary screen
        const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
        const int margin = 12;
        const int x = avail.right()  - win->width()  - margin;
        const int y = avail.top() + margin;
        SetWindowPos(hwnd, nullptr, x, y, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
};

std::unique_ptr<WindowOverlay> WindowOverlay::create()
{
    return std::make_unique<WindowsWindowOverlay>();
}

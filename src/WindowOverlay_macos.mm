#include "WindowOverlay.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#import <AppKit/AppKit.h>

class MacOSWindowOverlay final : public WindowOverlay {
public:
    void setup(QWindow *win) override {
        win->create();  // ensure native NSWindow exists before show()
        NSView *nsview = reinterpret_cast<NSView *>(win->winId());
        NSWindow *nswin = [nsview window];
        if (!nswin) return;

        [nswin setLevel: NSStatusWindowLevel + 1];
        [nswin setCollectionBehavior:
            NSWindowCollectionBehaviorCanJoinAllSpaces  |
            NSWindowCollectionBehaviorStationary        |
            NSWindowCollectionBehaviorIgnoresCycle];
        [nswin setHidesOnDeactivate: NO];
        // Allow hover events without requiring the window to be clicked first
        [nswin setAcceptsMouseMovedEvents: YES];

        // Position so right edge sits margin px from screen right.
        // Use the full panel width (not win->width which equals dotD at startup)
        // so the window stays fixed and the card expands leftward on hover.
        const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
        const int margin    = 12;
        const int panelW    = 320; // must match main.qml listW
        win->setPosition(avail.right() - panelW - margin,
                         avail.top()  + margin);
    }
};

std::unique_ptr<WindowOverlay> WindowOverlay::create()
{
    return std::make_unique<MacOSWindowOverlay>();
}

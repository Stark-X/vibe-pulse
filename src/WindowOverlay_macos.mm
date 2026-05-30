#include "WindowOverlay.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#import <AppKit/AppKit.h>

class MacOSWindowOverlay final : public WindowOverlay {
public:
    void setup(QWindow *win, const OverlayOptions &opts) override
    {
        win->create();  // ensure native NSWindow exists before show()
        NSView *nsview = reinterpret_cast<NSView *>(win->winId());
        NSWindow *nswin = [nsview window];
        if (!nswin) return;

        [nswin setLevel: NSStatusWindowLevel + 1];
        [nswin setCollectionBehavior:
            NSWindowCollectionBehaviorCanJoinAllSpaces    |
            NSWindowCollectionBehaviorStationary          |
            NSWindowCollectionBehaviorFullScreenAuxiliary |
            NSWindowCollectionBehaviorIgnoresCycle];
        [nswin setHidesOnDeactivate: NO];

        if (opts.ignoresMouseEvents)
            [nswin setIgnoresMouseEvents: YES];

        switch (opts.role) {
        case OverlayOptions::MainCard: {
            // Position top-right of primary screen
            const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
            const int margin = 12;
            win->setPosition(avail.right() - win->width() - margin,
                             avail.top() + margin);
            break;
        }
        case OverlayOptions::NotchLeftHud:
        case OverlayOptions::NotchRightHud:
            // Position set by C++ via NotchGeometry properties after setup
            break;
        }
    }
};

std::unique_ptr<WindowOverlay> WindowOverlay::create()
{
    return std::make_unique<MacOSWindowOverlay>();
}

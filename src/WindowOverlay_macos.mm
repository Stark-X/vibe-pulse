#include "WindowOverlay.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWindow>

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

// Swizzle constrainFrameRect:toScreen: on the actual class of a given NSWindow.
// Windows at NSScreenSaverWindowLevel bypass safe-area / notch clamping.
// Uses method_setImplementation (not isa/object_setClass) so Qt's KVO observers
// (registered on the original class) remain valid when setStyleMask: fires later.
static void swizzleConstrainFrameRect(NSWindow *nswin)
{
    static NSMutableSet *done = nil;
    static dispatch_once_t initOnce;
    dispatch_once(&initOnce, ^{ done = [NSMutableSet new]; });

    Class cls = [nswin class];
    NSString *name = NSStringFromClass(cls);
    @synchronized(done) {
        if ([done containsObject:name]) return;
        [done addObject:name];
    }

    SEL sel = @selector(constrainFrameRect:toScreen:);
    Method m = class_getInstanceMethod(cls, sel);
    if (!m) {
        FILE *f = fopen("/tmp/notch_swizzle.log", "a");
        if (f) { fprintf(f, "constrainFrameRect not found on %s\n", [name UTF8String]); fclose(f); }
        return;
    }

    IMP orig = method_getImplementation(m);
    IMP replacement = imp_implementationWithBlock(
        ^NSRect(NSWindow *self, NSRect frameRect, NSScreen *screen) {
            if (self.level >= NSScreenSaverWindowLevel)
                return frameRect; // notch HUD — allow any position, no clamping
            typedef NSRect (*Fn)(id, SEL, NSRect, NSScreen *);
            return ((Fn)orig)(self, sel, frameRect, screen);
        });
    method_setImplementation(m, replacement);

    FILE *f = fopen("/tmp/notch_swizzle.log", "a");
    if (f) { fprintf(f, "Swizzled constrainFrameRect on class: %s\n", [name UTF8String]); fclose(f); }
}

class MacOSWindowOverlay final : public WindowOverlay {
public:
    void setup(QWindow *win, const OverlayOptions &opts) override
    {
        win->create();  // ensure native NSWindow exists before show()
        NSView *nsview = reinterpret_cast<NSView *>(win->winId());
        NSWindow *nswin = [nsview window];
        if (!nswin) return;

        // Notch HUDs use NSScreenSaverWindowLevel (1000) so that
        // constrainFrameRect:toScreen: does NOT clamp them to the visible frame
        // (below menu bar). At lower levels macOS ignores setFrameTopLeftPoint:
        // and pushes the window to Y >= menuBarHeight regardless.
        NSWindowLevel level = (opts.role == OverlayOptions::NotchLeftHud ||
                               opts.role == OverlayOptions::NotchRightHud)
                              ? NSScreenSaverWindowLevel
                              : NSStatusWindowLevel + 1;

        [nswin setLevel: level];

        if (opts.role == OverlayOptions::NotchLeftHud ||
            opts.role == OverlayOptions::NotchRightHud)
            swizzleConstrainFrameRect(nswin);

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

    // Bypass Qt's menu-bar safe-area clamping by calling NSWindow directly.
    // x/y are Qt global logical coordinates (origin = primary screen top-left).
    void placeNotchHud(QWindow *win, int x, int y) override
    {
        NSView *nsview = reinterpret_cast<NSView *>(win->winId());
        if (!nsview) return;
        NSWindow *nswin = [nsview window];
        if (!nswin) return;

        // IMPORTANT: use screens[0] (always the primary screen, with menu bar),
        // NOT [NSScreen mainScreen] which returns the screen with keyboard focus
        // and could be the MacBook — giving wrong primaryTop and sending the
        // window to an offscreen Y position.
        NSScreen *primary = [[NSScreen screens] firstObject];
        if (!primary) return;
        CGFloat primaryTop = primary.frame.origin.y + primary.frame.size.height;
        CGFloat targetX = static_cast<CGFloat>(x);
        CGFloat targetY = primaryTop - static_cast<CGFloat>(y);

        // Re-set level: QML flags binding (completeCreate) fires after setup() and
        // resets NSWindow level to NSNormalWindowLevel (8), which breaks the
        // constrainFrameRect swizzle condition. Force it back before positioning.
        [nswin setLevel: NSScreenSaverWindowLevel];
        [nswin setFrameTopLeftPoint: NSMakePoint(targetX, targetY)];

        FILE *f = fopen("/tmp/notch_place.log", "a");
        if (f) {
            NSRect fr = nswin.frame;
            CGFloat actualTop = fr.origin.y + fr.size.height;
            fprintf(f, "placeNotchHud Qt(%d,%d) AppKit(%.0f,%.0f) level=%ld after topY=%.0f diff=%.0f%s\n",
                    x, y, targetX, targetY, (long)nswin.level,
                    actualTop, targetY - actualTop,
                    (targetY == actualTop) ? " OK" : " CLAMPED");
            fclose(f);
        }
    }
};

std::unique_ptr<WindowOverlay> WindowOverlay::create()
{
    return std::make_unique<MacOSWindowOverlay>();
}

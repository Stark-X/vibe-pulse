#pragma once
#include <memory>
#include <QMap>
#include <QRectF>
#include <QObject>
#include <QVariant>
#include <QVector>
#include <QWindow>

struct OverlayOptions {
    enum Role { MainCard, NotchLeftHud, NotchRightHud, NotchFusionWidget };
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

    // Update native hit-test regions for a fusion widget window.
    // Regions outside these areas pass mouse events through to the desktop.
    // Default is a no-op; macOS overrides with NSView hitTest swizzle.
    virtual void setHitTestRegions(QWindow *win, const QVector<QRectF> &regions) {
        Q_UNUSED(win); Q_UNUSED(regions);
    }

    // Place a fusion widget window at (x, y) with given width.
    // Combines positioning, level restoration, and width setting in one call.
    // macOS overrides to bypass safe-area clamping; default uses setPosition.
    virtual void placeFusionWindow(QWindow *win, qreal x, qreal y, qreal width) {
        win->setPosition(static_cast<int>(x), static_cast<int>(y));
        win->setWidth(static_cast<int>(width));
    }

    static std::unique_ptr<WindowOverlay> create();
};

// QML-accessible proxy for WindowOverlay methods.
// Exposed as "overlayProxy" context property so QML can call setHitTestRegions.
class OverlayProxy : public QObject {
    Q_OBJECT
public:
    explicit OverlayProxy(QObject *parent = nullptr) : QObject(parent) {}

    WindowOverlay *overlay = nullptr;
    QWindow *fusionWindow = nullptr;

    Q_INVOKABLE void setHitTestRegions(const QVariant &regionsVar) {
        if (!overlay || !fusionWindow) return;
        const QVariantList list = regionsVar.toList();
        QVector<QRectF> regions;
        for (const QVariant &v : list) {
            const QVariantMap m = v.toMap();
            regions.append(QRectF(
                m.value(QStringLiteral("x")).toReal(),
                m.value(QStringLiteral("y")).toReal(),
                m.value(QStringLiteral("width")).toReal(),
                m.value(QStringLiteral("height")).toReal()));
        }
        overlay->setHitTestRegions(fusionWindow, regions);
    }

    Q_INVOKABLE void placeFusionWindow(qreal x, qreal y, qreal width) {
        if (!overlay || !fusionWindow) return;
        overlay->placeFusionWindow(fusionWindow, x, y, width);
    }
};

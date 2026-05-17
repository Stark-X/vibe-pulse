#include "NotchGeometry.h"

#include <QGuiApplication>
#include <QScreen>
#include <QVariantMap>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>

NotchGeometry::NotchGeometry(QObject *parent)
    : QObject(parent)
{
    compute();

    [[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidChangeScreenParametersNotification
                    object:nil
                     queue:[NSOperationQueue mainQueue]
                usingBlock:^(NSNotification *) { refresh(); }];
}

QVariantList NotchGeometry::screenPositions() const
{
    QVariantList list;
    for (const auto &pos : m_screenPositions) {
        list.append(QVariantMap{
            {QStringLiteral("screenName"),    pos.screenName},
            {QStringLiteral("leftX"),         pos.leftX},
            {QStringLiteral("rightX"),        pos.rightX},
            {QStringLiteral("y"),             pos.y},
            {QStringLiteral("hasNotch"),      pos.hasNotch},
            {QStringLiteral("screenWidth"),   pos.screenWidth},
            {QStringLiteral("safeTop"),       pos.safeTop},
            {QStringLiteral("leftAreaWidth"), pos.leftAreaWidth},
            {QStringLiteral("rightAreaWidth"),pos.rightAreaWidth},
            {QStringLiteral("notchLeftX"),    pos.notchLeftX},
            {QStringLiteral("notchRightX"),   pos.notchRightX},
        });
    }
    return list;
}

void NotchGeometry::refresh()
{
    compute();
}

void NotchGeometry::compute()
{
    bool prevAvailable = m_available;
    QVector<ScreenHudPos> prevPositions = m_screenPositions;

    m_available = false;
    m_screenPositions.clear();

    // Debug: dump to /tmp/notch_geo.log (append so multiple compute() calls visible)
    FILE *dbg = fopen("/tmp/notch_geo.log", "a");
    auto LOG = [&](const char *fmt, ...) {
        if (!dbg) return;
        va_list ap; va_start(ap, fmt); vfprintf(dbg, fmt, ap); va_end(ap);
        fflush(dbg);
    };

    if (@available(macOS 12.0, *)) {
        // Use [NSScreen screens][0] for primary screen baseline (consistent with
        // WindowOverlay_macos.mm placeNotchHud).  [NSScreen mainScreen] returns
        // the screen with keyboard focus which can be an external display.
        NSArray<NSScreen *> *allScreens = [NSScreen screens];
        NSScreen *primaryScreen = allScreens.count > 0 ? allScreens[0] : [NSScreen mainScreen];
        CGFloat primaryTopInAppKit = primaryScreen.frame.origin.y + primaryScreen.frame.size.height;
        LOG("primaryTopInAppKit=%.0f\n", primaryTopInAppKit);

        for (NSScreen *screen in [NSScreen screens]) {
            ScreenHudPos pos;
            pos.screenName = QString::fromNSString(screen.localizedName);
            pos.hasNotch = false;

            NSRect frame = screen.frame;
            CGFloat safeTop = screen.safeAreaInsets.top;
            CGFloat leftAreaW = screen.auxiliaryTopLeftArea.size.width;
            CGFloat rightAreaW = screen.auxiliaryTopRightArea.size.width;

            // Convert AppKit screen origin to Qt coordinates (top-left origin)
            qreal screenX = static_cast<qreal>(frame.origin.x);
            qreal screenY = static_cast<qreal>(primaryTopInAppKit - frame.origin.y - frame.size.height);

            pos.screenX = screenX;
            pos.screenY = screenY;

            LOG("screen=%s frame=(%.0f,%.0f,%.0fx%.0f) safeTop=%.0f leftAreaW=%.0f rightAreaW=%.0f\n",
                pos.screenName.toUtf8().constData(),
                frame.origin.x, frame.origin.y, frame.size.width, frame.size.height,
                safeTop, leftAreaW, rightAreaW);
            LOG("  Qt coords: screenX=%.0f screenY=%.0f\n", screenX, screenY);

            if (safeTop > 0 && leftAreaW > 0 && rightAreaW > 0) {
                // Screen has a notch — HUDs sit flush at the top, flanking the notch.
                // Y=0 puts them in the notch band; placeNotchHud() bypasses macOS
                // safe-area clamping so the window actually reaches Y=0.
                pos.hasNotch = true;
                const qreal leftHudW   = 64;
                const qreal leftMargin  = 4;
                const qreal rightMargin = 4;

                pos.y      = screenY;  // top of screen (within notch band)
                pos.leftX  = screenX + static_cast<qreal>(leftAreaW  - leftHudW  - leftMargin);
                pos.rightX = screenX + static_cast<qreal>(frame.size.width - rightAreaW + rightMargin);

                // Fusion widget geometry
                pos.screenWidth    = static_cast<qreal>(frame.size.width);
                pos.safeTop        = static_cast<qreal>(safeTop);
                pos.leftAreaWidth  = static_cast<qreal>(leftAreaW);
                pos.rightAreaWidth = static_cast<qreal>(rightAreaW);
                pos.notchLeftX     = screenX + static_cast<qreal>(leftAreaW);
                pos.notchRightX    = screenX + static_cast<qreal>(frame.size.width - rightAreaW);
                LOG("  NOTCH: leftX=%.0f rightX=%.0f y=%.0f\n", pos.leftX, pos.rightX, pos.y);
            } else {
                // No notch — position HUDs at the very top of the screen.
                // The menu bar overlaps this area; at NSScreenSaverWindowLevel the
                // HUDs appear just above or flush with the menu bar.
                pos.y      = screenY;   // Y=0 relative to screen top (Qt global)
                pos.leftX  = screenX + 8;
                pos.rightX = screenX + static_cast<qreal>(frame.size.width) - 60 - 12;

                // Fusion widget geometry (no notch)
                pos.screenWidth    = static_cast<qreal>(frame.size.width);
                pos.safeTop        = static_cast<qreal>(safeTop);
                pos.leftAreaWidth  = static_cast<qreal>(leftAreaW);
                pos.rightAreaWidth = static_cast<qreal>(rightAreaW);
                pos.notchLeftX     = screenX + static_cast<qreal>(leftAreaW);
                pos.notchRightX    = screenX + static_cast<qreal>(frame.size.width - rightAreaW);
                LOG("  NO-NOTCH: leftX=%.0f rightX=%.0f y=%.0f\n", pos.leftX, pos.rightX, pos.y);
            }

            m_screenPositions.append(pos);
            m_available = true;
        }
    }

    if (dbg) fclose(dbg);

    if (m_available != prevAvailable || m_screenPositions.size() != prevPositions.size())
        emit geometryChanged();
    else {
        for (int i = 0; i < m_screenPositions.size(); ++i) {
            const auto &a = m_screenPositions[i], &b = prevPositions[i];
            if (a.screenName != b.screenName || a.screenX != b.screenX ||
                a.screenY != b.screenY || a.leftX != b.leftX ||
                a.rightX != b.rightX || a.y != b.y || a.hasNotch != b.hasNotch ||
                a.screenWidth != b.screenWidth || a.safeTop != b.safeTop ||
                a.leftAreaWidth != b.leftAreaWidth || a.rightAreaWidth != b.rightAreaWidth ||
                a.notchLeftX != b.notchLeftX || a.notchRightX != b.notchRightX) {
                emit geometryChanged();
                break;
            }
        }
    }
}

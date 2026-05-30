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
            {QStringLiteral("screenName"), pos.screenName},
            {QStringLiteral("leftX"),      pos.leftX},
            {QStringLiteral("rightX"),     pos.rightX},
            {QStringLiteral("y"),          pos.y},
            {QStringLiteral("hasNotch"),   pos.hasNotch},
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

    if (@available(macOS 12.0, *)) {
        // Determine the global coordinate system baseline (top of primary screen in Qt coords)
        NSScreen *primaryScreen = [NSScreen mainScreen];
        CGFloat primaryTopInAppKit = primaryScreen.frame.origin.y + primaryScreen.frame.size.height;

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

            if (safeTop > 0 && leftAreaW > 0 && rightAreaW > 0) {
                // Screen has a notch — position HUDs flanking it
                pos.hasNotch = true;
                const qreal hudHeight = 28;
                const qreal leftMargin = 4;
                const qreal rightMargin = 4;

                pos.y = screenY + static_cast<qreal>((safeTop - hudHeight) / 2.0 + 2);
                pos.leftX = screenX + static_cast<qreal>(leftAreaW - 48 - leftMargin);
                pos.rightX = screenX + static_cast<qreal>(frame.size.width - rightAreaW + rightMargin);
            } else {
                // No notch — position HUDs in the menu bar area (top of screen)
                // Menu bar height is typically ~24pt; safeAreaInsets.top gives it when no notch
                CGFloat menuBarH = safeTop > 0 ? safeTop : 24.0;
                const qreal hudHeight = 28;
                const qreal rightMargin = 12;

                pos.y = screenY + static_cast<qreal>((menuBarH - hudHeight) / 2.0 + 1);
                // Left HUD at left edge of screen
                pos.leftX = screenX + 8;
                // Right HUD near right edge of screen
                pos.rightX = screenX + static_cast<qreal>(frame.size.width) - 32 - rightMargin;
            }

            m_screenPositions.append(pos);
            m_available = true;
        }
    }

    if (m_available != prevAvailable || m_screenPositions.size() != prevPositions.size())
        emit geometryChanged();
    else {
        for (int i = 0; i < m_screenPositions.size(); ++i) {
            const auto &a = m_screenPositions[i], &b = prevPositions[i];
            if (a.screenName != b.screenName || a.leftX != b.leftX ||
                a.rightX != b.rightX || a.y != b.y || a.hasNotch != b.hasNotch) {
                emit geometryChanged();
                break;
            }
        }
    }
}

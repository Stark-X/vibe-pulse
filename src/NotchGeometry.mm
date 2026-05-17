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

    if (@available(macOS 12.0, *)) {
        NSArray<NSScreen *> *allScreens = [NSScreen screens];
        NSScreen *primaryScreen = allScreens.count > 0 ? allScreens[0] : [NSScreen mainScreen];
        CGFloat primaryTopInAppKit = primaryScreen.frame.origin.y + primaryScreen.frame.size.height;

        for (NSScreen *screen in [NSScreen screens]) {
            ScreenHudPos pos;
            pos.screenName = QString::fromNSString(screen.localizedName);
            pos.hasNotch = false;

            NSRect frame = screen.frame;
            CGFloat safeTop = screen.safeAreaInsets.top;
            CGFloat leftAreaW = screen.auxiliaryTopLeftArea.size.width;
            CGFloat rightAreaW = screen.auxiliaryTopRightArea.size.width;

            qreal screenX = static_cast<qreal>(frame.origin.x);
            qreal screenY = static_cast<qreal>(primaryTopInAppKit - frame.origin.y - frame.size.height);

            pos.screenX = screenX;
            pos.screenY = screenY;

            if (safeTop > 0 && leftAreaW > 0 && rightAreaW > 0) {
                pos.hasNotch = true;
                const qreal leftHudW   = 64;
                const qreal leftMargin  = 4;
                const qreal rightMargin = 4;

                pos.y      = screenY;
                pos.leftX  = screenX + static_cast<qreal>(leftAreaW  - leftHudW  - leftMargin);
                pos.rightX = screenX + static_cast<qreal>(frame.size.width - rightAreaW + rightMargin);

                pos.screenWidth    = static_cast<qreal>(frame.size.width);
                pos.safeTop        = static_cast<qreal>(safeTop);
                pos.leftAreaWidth  = static_cast<qreal>(leftAreaW);
                pos.rightAreaWidth = static_cast<qreal>(rightAreaW);
                pos.notchLeftX     = screenX + static_cast<qreal>(leftAreaW);
                pos.notchRightX    = screenX + static_cast<qreal>(frame.size.width - rightAreaW);
            } else {
                pos.y      = screenY;
                pos.leftX  = screenX + 8;
                pos.rightX = screenX + static_cast<qreal>(frame.size.width) - 60 - 12;

                pos.screenWidth    = static_cast<qreal>(frame.size.width);
                pos.safeTop        = static_cast<qreal>(safeTop);
                pos.leftAreaWidth  = static_cast<qreal>(leftAreaW);
                pos.rightAreaWidth = static_cast<qreal>(rightAreaW);
                pos.notchLeftX     = screenX + static_cast<qreal>(leftAreaW);
                pos.notchRightX    = screenX + static_cast<qreal>(frame.size.width - rightAreaW);
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

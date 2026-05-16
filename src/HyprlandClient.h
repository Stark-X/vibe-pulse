#pragma once
#include "AgentInfo.h"
#include <QString>
#include <QVector>

struct HyprWindow {
    QString address;
    qint64  pid = 0;
    QString cls;
    QString title;
    int     x = 0;
    int     y = 0;
};

class HyprlandClient
{
public:
    static bool available();
    static QVector<HyprWindow> clients();
    static QString focusWindow(const QString &address);
    // Resize and immediately move to (x, y) in one --batch call.
    // resizewindowpixel scales from center on floating windows; the move
    // re-anchors the top-left corner back to the pre-resize position.
    static void resizeWindow(const QString &address, int width, int height, int x, int y);
    // Resize only, no position correction (fallback when position is unknown).
    static void resizeWindow(const QString &address, int width, int height);
    static QString findWindowAddress(quint32 agentPid,
                                     const QVector<HyprWindow> &wins);
};

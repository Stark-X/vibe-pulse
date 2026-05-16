#pragma once
#include "AgentInfo.h"
#include <QString>
#include <QVector>

struct HyprWindow {
    QString address;
    qint64  pid = 0;
    QString cls;
    QString title;
};

class HyprlandClient
{
public:
    static bool available();
    static QVector<HyprWindow> clients();
    static QString focusWindow(const QString &address);
    // Resize the window and immediately move it to (x, y) in a single --batch call.
    // Required because resizewindowpixel on floating windows scales from center;
    // the follow-up movewindowpixel re-anchors the window to the correct position.
    static void resizeWindow(const QString &address, int width, int height, int x, int y);
    static QString findWindowAddress(quint32 agentPid,
                                     const QVector<HyprWindow> &wins);
};

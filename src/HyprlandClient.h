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
    static void resizeWindow(const QString &address, int width, int height);
    static QString findWindowAddress(quint32 agentPid,
                                     const QVector<HyprWindow> &wins);
};

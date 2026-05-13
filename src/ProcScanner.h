#pragma once
#include "AgentInfo.h"
#include <QObject>
#include <QTimer>
#include <QVector>

class ProcScanner : public QObject
{
    Q_OBJECT
public:
    explicit ProcScanner(QObject *parent = nullptr);
    void start();
    void stop();

    // Exposed for direct call if needed
    static QVector<AgentInfo> scanAll();

signals:
    void snapshotReady(QVector<AgentInfo> agents);

private:
    static bool    readStatus(quint32 pid, quint32 &outTgid, quint32 &outPpid);
    static QString readComm(quint32 pid);
    static QString readExe(quint32 pid);
    static QString readCwd(quint32 pid);
    static QStringList readCmdline(quint32 pid);
    static quint32 ppidOf(quint32 pid);
    static void    dedup(QVector<AgentInfo> &agents);

    QTimer m_timer;
};

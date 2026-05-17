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

    static QVector<AgentInfo> scanAll();

signals:
    void snapshotReady(QVector<AgentInfo> agents);

private:
    static void dedup(QVector<AgentInfo> &agents);

    QTimer m_timer;
};

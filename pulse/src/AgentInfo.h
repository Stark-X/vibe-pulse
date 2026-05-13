#pragma once
#include <QMetaType>
#include <QString>
#include <QVector>

struct AgentInfo {
    QString name;
    QString toolType;
    quint32 pid       = 0;
    QString status;           // "running" | "exited"
    QString cwd;
    QString sessionId;
    QString sessionName;
    bool    sessionBusy = true;
    QString currentStep;
    QString windowAddress;

    bool operator==(const AgentInfo &o) const noexcept {
        return pid == o.pid && toolType == o.toolType;
    }
    bool operator!=(const AgentInfo &o) const noexcept { return !(*this == o); }

    // Full data comparison (operator== is identity-only: pid+toolType).
    bool dataEquals(const AgentInfo &o) const noexcept {
        return name == o.name && status == o.status && cwd == o.cwd
            && sessionId == o.sessionId && sessionName == o.sessionName
            && sessionBusy == o.sessionBusy && currentStep == o.currentStep
            && windowAddress == o.windowAddress;
    }
};

Q_DECLARE_METATYPE(AgentInfo)
Q_DECLARE_METATYPE(QVector<AgentInfo>)

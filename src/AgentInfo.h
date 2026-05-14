#pragma once
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

enum class PulseState {
    Idle       = 0,
    Working    = 1,
    Permission = 2,
    Question   = 3,
    Plan       = 4,
};

struct PermissionLine {
    QString kind;
    QString text;

    bool operator==(const PermissionLine &o) const noexcept {
        return kind == o.kind && text == o.text;
    }
};

struct AgentInfo {
    QString name;
    QString toolType;
    quint32 pid       = 0;
    QString status;
    QString cwd;
    QString sessionId;
    QString sessionName;
    bool    sessionBusy = true;
    QString currentStep;
    QString windowAddress;

    PulseState  pulseState = PulseState::Idle;
    QString     questionPrompt;
    QStringList questionOptions;
    QString     planTitle;
    QString     planMarkdown;
    QString     planHtml;
    QString     permissionTool;
    QString     permissionTarget;
    QString     permissionDesc;
    QString     interactionId;

    bool operator==(const AgentInfo &o) const noexcept {
        return pid == o.pid && toolType == o.toolType;
    }
    bool operator!=(const AgentInfo &o) const noexcept { return !(*this == o); }

    bool dataEquals(const AgentInfo &o) const noexcept {
        return name == o.name && status == o.status && cwd == o.cwd
            && sessionId == o.sessionId && sessionName == o.sessionName
            && sessionBusy == o.sessionBusy && currentStep == o.currentStep
            && windowAddress == o.windowAddress
            && pulseState == o.pulseState
            && questionPrompt == o.questionPrompt
            && questionOptions == o.questionOptions
            && planTitle == o.planTitle
            && planMarkdown == o.planMarkdown
            && planHtml == o.planHtml
            && permissionTool == o.permissionTool
            && permissionTarget == o.permissionTarget
            && permissionDesc == o.permissionDesc
            && interactionId == o.interactionId;
    }
};

Q_DECLARE_METATYPE(PulseState)
Q_DECLARE_METATYPE(PermissionLine)
Q_DECLARE_METATYPE(AgentInfo)
Q_DECLARE_METATYPE(QVector<AgentInfo>)

#pragma once
#include "AgentInfo.h"

#include <QString>
#include <QStringList>

struct ClaudeMeta {
    QString sessionId;
    QString sessionName;
    bool    sessionBusy = true;
    QString currentStep;
    QString cwd;

    PulseState  pulseState = PulseState::Idle;
    QString     questionPrompt;
    QStringList questionOptions;
    QString     planTitle;
    QString     planMarkdown;
    QString     planHtml;
    QString     permissionTool;
    QString     permissionTarget;
    QString     interactionId;
};

class ClaudeMetaReader
{
public:
    static ClaudeMeta read(quint32 pid, const QString &cwd,
                            QStringList *watchPaths = nullptr);
};

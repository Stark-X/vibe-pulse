#pragma once
#include "AgentInfo.h"

#include <QString>
#include <QStringList>

struct CodexMeta {
    QString    sessionId;
    QString    sessionName;
    bool       sessionBusy = true;
    QString    currentStep;
    PulseState pulseState = PulseState::Idle;
    int        contextUsed  = 0;
    int        contextLimit = 0;
};

class CodexMetaReader
{
public:
    static CodexMeta read(quint32 pid, QStringList *watchPaths = nullptr);
};

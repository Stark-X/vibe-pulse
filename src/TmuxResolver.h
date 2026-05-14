#pragma once
#include <QtGlobal>
#include <QString>
#include <optional>

struct TmuxPaneInfo {
    quint32 terminalPid = 0;
    QString sessionId;
    QString sessionName;
    int     windowIndex = -1;
    int     paneIndex   = -1;
    QString clientTty;
    QString tmuxTarget;   // "$N:windowIndex.paneIndex"
};

class TmuxResolver
{
public:
    static std::optional<TmuxPaneInfo> findPaneInfo(quint32 agentPid);
};

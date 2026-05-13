#pragma once
#include <QtGlobal>
#include <optional>

class TmuxResolver
{
public:
    // Returns terminal PID for agent running inside tmux, or nullopt
    static std::optional<quint32> findTerminalPid(quint32 agentPid);
};

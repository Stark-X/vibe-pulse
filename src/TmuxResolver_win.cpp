#include "TmuxResolver.h"
#include "ProcessTree.h"

#include <climits>

#include <QProcess>
#include <QStringList>

// On Windows, tmux lives inside WSL. We detect WSL agents by their Win32
// ancestor chain containing wsl.exe, then proxy all tmux queries via wsl.exe.
std::optional<TmuxPaneInfo> TmuxResolver::findPaneInfo(quint32 agentPid)
{
    if (agentPid <= 1)
        return std::nullopt;

    // Check whether this agent has wsl.exe in its Win32 ancestor chain
    const QVector<quint32> chain = ProcessTree::ancestorChain(agentPid);
    bool inWsl = false;
    for (quint32 p : chain) {
        if (ProcessTree::comm(p).compare(QStringLiteral("wsl.exe"),
                                         Qt::CaseInsensitive) == 0) {
            inWsl = true;
            break;
        }
    }
    if (!inWsl)
        return std::nullopt;

    // Proxy tmux list-panes through wsl.exe
    auto wslTmux = [](const QStringList &tmuxArgs) -> QProcess * {
        QStringList args = { QStringLiteral("--") };
        args << QStringLiteral("tmux") << tmuxArgs;
        auto *p = new QProcess();
        p->start(QStringLiteral("wsl.exe"), args);
        return p;
    };

    QProcess *panes = wslTmux({
        QStringLiteral("list-panes"), QStringLiteral("-a"), QStringLiteral("-F"),
        QStringLiteral("#{pane_pid}\t#{session_id}\t#{session_name}\t#{window_index}\t#{pane_index}")
    });
    if (!panes->waitForFinished(2000)) { panes->kill(); panes->waitForFinished(200); delete panes; return std::nullopt; }
    if (panes->exitCode() != 0) { delete panes; return std::nullopt; }

    // WSL PIDs in panes output are WSL-internal PIDs; we can't directly compare
    // them with Win32 PIDs. Match by looking up the Win32 PID of the agent inside
    // WSL via /proc — best-effort only.
    int     bestHop       = INT_MAX;
    QString bestSessionId;
    QString bestSessionName;
    int     bestWindowIdx = -1;
    int     bestPaneIdx   = -1;

    for (const QByteArray &raw : panes->readAllStandardOutput().split('\n')) {
        const QStringList parts = QString::fromLocal8Bit(raw).split(QLatin1Char('\t'));
        if (parts.size() < 5) continue;
        const quint32 panePid = parts[0].toUInt();
        if (panePid == 0) continue;
        // Accept first pane that matches exe basename heuristically (WSL PID ≠ Win32 PID)
        if (bestHop == INT_MAX) {
            bestHop         = 0;
            bestSessionId   = parts[1];
            bestSessionName = parts[2];
            bestWindowIdx   = parts[3].toInt();
            bestPaneIdx     = parts[4].toInt();
        }
    }
    delete panes;

    if (bestHop == INT_MAX)
        return std::nullopt;

    TmuxPaneInfo info;
    info.sessionId   = bestSessionId;
    info.sessionName = bestSessionName;
    info.windowIndex = bestWindowIdx;
    info.paneIndex   = bestPaneIdx;
    info.tmuxTarget  = QStringLiteral("%1:%2.%3")
                           .arg(bestSessionId)
                           .arg(bestWindowIdx)
                           .arg(bestPaneIdx);
    return info;
}

#include "TmuxResolver.h"
#include "ProcessTree.h"

#include <climits>

#include <QProcess>
#include <QStringList>

std::optional<TmuxPaneInfo> TmuxResolver::findPaneInfo(quint32 agentPid)
{
    if (agentPid <= 1)
        return std::nullopt;

    // Build PPID chain; detect tmux ancestor
    const QVector<quint32> chain = ProcessTree::ancestorChain(agentPid);
    bool inTmux = false;
    for (quint32 p : chain) {
        if (ProcessTree::comm(p).startsWith(QStringLiteral("tmux"))) {
            inTmux = true;
            break;
        }
    }
    if (!inTmux)
        return std::nullopt;

    // list-panes: find the pane containing agentPid (nearest ancestor wins)
    QProcess panes;
    panes.start(QStringLiteral("tmux"),
                {QStringLiteral("list-panes"), QStringLiteral("-a"),
                 QStringLiteral("-F"),
                 QStringLiteral("#{pane_pid}\t#{session_id}\t#{session_name}\t#{window_index}\t#{pane_index}")});
    if (!panes.waitForFinished(1000)) { panes.kill(); panes.waitForFinished(100); return std::nullopt; }
    if (panes.exitCode() != 0)
        return std::nullopt;

    int     bestHop       = INT_MAX;
    QString bestSessionId;
    QString bestSessionName;
    int     bestWindowIdx = -1;
    int     bestPaneIdx   = -1;

    for (const QByteArray &raw : panes.readAllStandardOutput().split('\n')) {
        const QStringList parts = QString::fromLocal8Bit(raw).split(QLatin1Char('\t'));
        if (parts.size() < 5)
            continue;
        const quint32 panePid = parts[0].toUInt();
        if (panePid == 0)
            continue;

        const int hop = chain.indexOf(panePid);
        if (hop >= 0 && hop < bestHop) {
            bestHop         = hop;
            bestSessionId   = parts[1];
            bestSessionName = parts[2];
            bestWindowIdx   = parts[3].toInt();
            bestPaneIdx     = parts[4].toInt();
        }
    }

    if (bestHop == INT_MAX)
        return std::nullopt;

    // list-clients: find first valid client attached to this session
    QProcess clients;
    clients.start(QStringLiteral("tmux"),
                  {QStringLiteral("list-clients"), QStringLiteral("-t"), bestSessionId,
                   QStringLiteral("-F"), QStringLiteral("#{client_pid}\t#{client_tty}")});
    if (!clients.waitForFinished(1000)) { clients.kill(); clients.waitForFinished(100); return std::nullopt; }
    if (clients.exitCode() != 0)
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

    for (const QByteArray &raw : clients.readAllStandardOutput().split('\n')) {
        const QStringList parts = QString::fromLocal8Bit(raw).split(QLatin1Char('\t'));
        if (parts.size() < 2)
            continue;
        const quint32 cpid = parts[0].toUInt();
        const QString tty  = parts[1].trimmed();
        if (cpid > 1 && !tty.isEmpty()) {
            info.terminalPid = cpid;
            info.clientTty   = tty;
            return info;
        }
    }

    return std::nullopt;
}

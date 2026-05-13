#include "TmuxResolver.h"

#include <QFile>
#include <QProcess>
#include <QStringList>

static QString commOf(quint32 pid)
{
    QFile f(QStringLiteral("/proc/%1/comm").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLocal8Bit(f.readAll()).trimmed();
}

static quint32 ppidOf(quint32 pid)
{
    QFile f(QStringLiteral("/proc/%1/status").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    for (const QByteArray &line : f.readAll().split('\n'))
        if (line.startsWith("PPid:"))
            return line.mid(5).trimmed().toUInt();
    return 0;
}

std::optional<quint32> TmuxResolver::findTerminalPid(quint32 agentPid)
{
    // Walk PPid chain; look for a tmux: server or tmux process
    quint32 cur = agentPid;
    bool inTmux = false;
    for (int i = 0; i < 16; ++i) {
        cur = ppidOf(cur);
        if (cur == 0 || cur == 1)
            break;
        QString c = commOf(cur);
        if (c.startsWith(QStringLiteral("tmux"))) {
            inTmux = true;
            break;
        }
    }
    if (!inTmux)
        return std::nullopt;

    // tmux list-panes: find which session contains agentPid
    QProcess panes;
    panes.start(QStringLiteral("tmux"),
                {QStringLiteral("list-panes"), QStringLiteral("-a"),
                 QStringLiteral("-F"),
                 QStringLiteral("#{pane_pid}\t#{session_id}\t#{session_name}")});
    if (!panes.waitForFinished(1000))
        return std::nullopt;

    QString sessionId;
    for (const QByteArray &raw : panes.readAllStandardOutput().split('\n')) {
        QStringList parts = QString::fromLocal8Bit(raw).split(QLatin1Char('\t'));
        if (parts.size() < 2)
            continue;
        quint32 panePid = parts[0].toUInt();
        if (panePid == 0)
            continue;
        // check if agentPid is in the PPid chain of panePid
        quint32 c = agentPid;
        bool found = (panePid == agentPid);
        for (int i = 0; !found && i < 8; ++i) {
            c = ppidOf(c);
            if (c == panePid) found = true;
        }
        if (found) {
            sessionId = parts[1];
            break;
        }
    }
    if (sessionId.isEmpty())
        return std::nullopt;

    // tmux list-clients: get terminal PID for this session
    QProcess clients;
    clients.start(QStringLiteral("tmux"),
                  {QStringLiteral("list-clients"), QStringLiteral("-t"), sessionId,
                   QStringLiteral("-F"), QStringLiteral("#{client_pid}")});
    if (!clients.waitForFinished(1000))
        return std::nullopt;

    for (const QByteArray &raw : clients.readAllStandardOutput().split('\n')) {
        quint32 cpid = raw.trimmed().toUInt();
        if (cpid > 1)
            return cpid;
    }
    return std::nullopt;
}

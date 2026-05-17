#include "CodexMetaReader.h"
#include "ProcessTree.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>

// Find the session JSONL file that the Codex process has open.
// Codex keeps a write fd on ~/.codex/sessions/<Y>/<M>/<D>/rollout-*.jsonl.
//
// Strategy:
//   1. Ask ProcessTree for an open fd matching the path pattern (Linux/macOS).
//   2. Fall back to scanning ~/.codex/sessions for the most-recently-modified
//      rollout JSONL (Windows, or when fd enumeration is denied).
//      Caveat: with multiple concurrent Codex processes the fallback picks the
//      globally newest file, which may belong to a different process.
static QString findSessionFile(quint32 pid)
{
    const QString viaFd = ProcessTree::openFileMatching(
        pid, QStringLiteral("/.codex/sessions/"), QStringLiteral(".jsonl"));
    if (!viaFd.isEmpty())
        return viaFd;

    const QString base = QDir::homePath() + QStringLiteral("/.codex/sessions");
    QDirIterator it(base, {QStringLiteral("rollout-*.jsonl")},
                    QDir::Files, QDirIterator::Subdirectories);
    QString newestPath;
    QDateTime newestMtime;
    while (it.hasNext()) {
        const QString p = it.next();
        const QDateTime m = QFileInfo(p).lastModified();
        if (newestPath.isEmpty() || m > newestMtime) {
            newestPath  = p;
            newestMtime = m;
        }
    }
    return newestPath;
}

// Summarise a function_call event into a short display string.
static QString summarise(const QJsonObject &payload)
{
    const QString name = payload.value(QStringLiteral("name")).toString();
    if (name.isEmpty())
        return {};

    const QByteArray argsRaw =
        payload.value(QStringLiteral("arguments")).toString().toUtf8();
    const QJsonObject args = QJsonDocument::fromJson(argsRaw).object();

    // exec_command: show the shell command (first line only)
    if (name == QStringLiteral("exec_command")) {
        QString cmd = args.value(QStringLiteral("cmd")).toString();
        if (!cmd.isEmpty())
            return cmd.section(QLatin1Char('\n'), 0, 0);
    }

    // file-related tools: show filename
    for (const char *key : {"path", "file_path", "filename"}) {
        const QString val = args.value(QLatin1String(key)).toString();
        if (!val.isEmpty())
            return name + QStringLiteral(" · ") + QFileInfo(val).fileName();
    }

    return name;
}

// Read session name from ~/.codex/session_index.jsonl by session ID.
static QString readSessionName(const QString &sessionId, QStringList *watchPaths)
{
    const QString indexPath =
        QDir::homePath() + QStringLiteral("/.codex/session_index.jsonl");
    QFile f(indexPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    if (watchPaths) *watchPaths << indexPath;

    // Scan all lines (file is append-only; last match wins for updated names)
    QString name;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine().trimmed();
        if (line.isEmpty()) continue;
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;
        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("id")).toString() == sessionId) {
            const QString n = obj.value(QStringLiteral("thread_name")).toString();
            if (!n.isEmpty())
                name = n;
        }
    }
    return name;
}

CodexMeta CodexMetaReader::read(quint32 pid, QStringList *watchPaths)
{
    CodexMeta m;

    const QString sessionFile = findSessionFile(pid);
    if (sessionFile.isEmpty())
        return m;

    QFile f(sessionFile);
    if (!f.open(QIODevice::ReadOnly))
        return m;
    if (watchPaths) *watchPaths << sessionFile;

    const qint64 size = f.size();
    const qint64 tailSize = 16384;
    if (size > tailSize)
        f.seek(size - tailSize);

    QByteArray tail = f.readAll();
    if (size > tailSize) {
        const int nl = tail.indexOf('\n');
        if (nl >= 0) tail = tail.mid(nl + 1);
    }

    // Extract session ID from the filename: rollout-<timestamp>-<UUID>.jsonl
    // UUID is always the last 36 characters of the base name before ".jsonl".
    {
        const QString base = QFileInfo(sessionFile).completeBaseName(); // strip .jsonl
        if (base.length() > 36)
            m.sessionId = base.right(36);
    }

    if (!m.sessionId.isEmpty())
        m.sessionName = readSessionName(m.sessionId, watchPaths);

    // Scan lines in order; track last function_call command and whether a
    // task_complete arrived after it (Codex finished turn / asking question).
    QString lastCmd;
    QString lastAgentMsg;
    bool taskCompletedAfterCmd = false;
    int contextUsed  = 0;
    int contextLimit = 0;

    for (const QByteArray &raw : tail.split('\n')) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;

        const QJsonObject obj = doc.object();
        const QString objType = obj.value(QStringLiteral("type")).toString();
        const QJsonObject payload = obj.value(QStringLiteral("payload")).toObject();
        const QString ptype = payload.value(QStringLiteral("type")).toString();

        if (objType == QStringLiteral("event_msg") && ptype == QStringLiteral("token_count")) {
            const QJsonObject info = payload.value(QStringLiteral("info")).toObject();
            const QJsonObject lastUsage = info.value(QStringLiteral("last_token_usage")).toObject();
            const int used  = lastUsage.value(QStringLiteral("input_tokens")).toInt();
            const int limit = info.value(QStringLiteral("model_context_window")).toInt();
            if (used > 0 && limit > 0) {
                contextUsed  = used;
                contextLimit = limit;
            }
            continue;
        }

        if (objType != QStringLiteral("response_item"))
            continue;

        if (ptype == QStringLiteral("function_call")) {
            const QString step = summarise(payload);
            if (!step.isEmpty()) {
                lastCmd = step;
                taskCompletedAfterCmd = false;
            }
        } else if (ptype == QStringLiteral("task_complete")) {
            // Turn finished — capture what Codex said (may be a question)
            const QString msg = payload.value(QStringLiteral("last_agent_message")).toString();
            lastAgentMsg = msg.section(QLatin1Char('\n'), 0, 0).left(100);
            taskCompletedAfterCmd = true;
        }
    }

    m.contextUsed  = contextUsed;
    m.contextLimit = contextLimit;

    // If the turn completed after the last command, Codex is now waiting for
    // user input (idle or asking a question).  Show the agent's last message
    // so the user can see what Codex said/asked, otherwise show the last command.
    if (taskCompletedAfterCmd)
        m.currentStep = lastAgentMsg;
    else
        m.currentStep = lastCmd;

    m.sessionBusy = !taskCompletedAfterCmd;
    m.pulseState  = m.sessionBusy ? PulseState::Working : PulseState::Idle;

    return m;
}

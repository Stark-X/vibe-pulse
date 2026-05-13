#include "CodexMetaReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QJsonDocument>
#include <QJsonObject>

// Find the session JSONL file that the Codex process has open.
// Codex keeps a write fd on ~/.codex/sessions/<Y>/<M>/<D>/rollout-*.jsonl.
static QString findSessionFile(quint32 pid)
{
    const QString fdDir = QStringLiteral("/proc/%1/fd").arg(pid);
    const QDir dir(fdDir);
    if (!dir.exists())
        return {};

    for (const QString &entry : dir.entryList(QDir::System | QDir::Files)) {
        const QString link = QFileInfo(fdDir + QLatin1Char('/') + entry).symLinkTarget();
        if (link.contains(QStringLiteral("/.codex/sessions/")) && link.endsWith(QStringLiteral(".jsonl")))
            return link;
    }
    return {};
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
    const qint64 tailSize = 4096;
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

    for (const QByteArray &raw : tail.split('\n')) {
        const QByteArray line = raw.trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;

        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("response_item"))
            continue;

        const QJsonObject payload = obj.value(QStringLiteral("payload")).toObject();
        if (payload.value(QStringLiteral("type")).toString() != QStringLiteral("function_call"))
            continue;

        const QString step = summarise(payload);
        if (!step.isEmpty())
            m.currentStep = step;  // keep last one found
    }

    return m;
}

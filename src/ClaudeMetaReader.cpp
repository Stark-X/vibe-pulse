#include "ClaudeMetaReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

static QString encodeCwd(const QString &cwd)
{
    // mirrors Rust: cwd.replace('/', '-')
    QString enc = cwd;
    enc.replace(QLatin1Char('/'), QLatin1Char('-'));
    return enc;
}

static QString lastToolUse(const QByteArray &tail)
{
    // Split into lines, iterate in reverse, find last assistant tool_use
    QList<QByteArray> lines = tail.split('\n');
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QByteArray &raw = lines[i].trimmed();
        if (raw.isEmpty())
            continue;
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (err.error != QJsonParseError::NoError)
            continue;
        QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("assistant"))
            continue;
        QJsonArray content = obj.value(QStringLiteral("message"))
                                 .toObject()
                                 .value(QStringLiteral("content"))
                                 .toArray();
        for (int j = content.size() - 1; j >= 0; --j) {
            QJsonObject item = content[j].toObject();
            if (item.value(QStringLiteral("type")).toString() != QStringLiteral("tool_use"))
                continue;
            QString tool = item.value(QStringLiteral("name")).toString();
            QJsonObject input = item.value(QStringLiteral("input")).toObject();
            QString detail = input.value(QStringLiteral("description")).toString();
            if (detail.isEmpty()) detail = input.value(QStringLiteral("command")).toString();
            if (detail.isEmpty()) detail = input.value(QStringLiteral("path")).toString();
            if (!detail.isEmpty()) {
                detail = detail.section(QLatin1Char('\n'), 0, 0); // first line only
                return tool + QStringLiteral(" · ") + detail;
            }
            return tool;
        }
    }
    return {};
}

ClaudeMeta ClaudeMetaReader::read(quint32 pid, const QString &cwd,
                                   QStringList *watchPaths)
{
    ClaudeMeta m;

    const QString home = QDir::homePath();
    if (home.isEmpty())
        return m;

    // Session file — try PID-named file first, then search by cwd fallback
    QString sessionPath =
        QStringLiteral("%1/.claude/sessions/%2.json").arg(home).arg(pid);

    if (!QFileInfo::exists(sessionPath)) {
        // The detected "claude" launcher has a different PID than the worker
        // that writes the session file — find by matching cwd instead.
        const QDir sessDir(QStringLiteral("%1/.claude/sessions").arg(home));
        qint64 bestUpdated = 0;
        for (const QString &entry : sessDir.entryList({QStringLiteral("*.json")}, QDir::Files)) {
            const QString candidate = sessDir.filePath(entry);
            QFile cf(candidate);
            if (!cf.open(QIODevice::ReadOnly)) continue;
            QJsonParseError pe;
            QJsonDocument cd = QJsonDocument::fromJson(cf.readAll(), &pe);
            if (pe.error != QJsonParseError::NoError || !cd.isObject()) continue;
            QJsonObject co = cd.object();
            if (co.value(QStringLiteral("cwd")).toString() != cwd) continue;
            const qint64 updated = static_cast<qint64>(co.value(QStringLiteral("updatedAt")).toDouble());
            if (updated > bestUpdated) {
                bestUpdated = updated;
                sessionPath = candidate;
            }
        }
    }

    QFile sf(sessionPath);
    if (!sf.open(QIODevice::ReadOnly))
        return m;
    if (watchPaths) *watchPaths << sessionPath;

    QJsonParseError err;
    QJsonDocument sdoc = QJsonDocument::fromJson(sf.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !sdoc.isObject())
        return m;

    QJsonObject sobj = sdoc.object();
    m.sessionId   = sobj.value(QStringLiteral("sessionId")).toString();
    m.sessionName = sobj.value(QStringLiteral("name")).toString();
    m.cwd         = sobj.value(QStringLiteral("cwd")).toString();
    const QString status = sobj.value(QStringLiteral("status")).toString();
    m.sessionBusy = (status != QStringLiteral("idle"));

    if (m.sessionId.isEmpty())
        return m;

    // Waiting for permission — use waitingFor directly, skip JSONL read
    if (status == QStringLiteral("waiting")) {
        const QString wf = sobj.value(QStringLiteral("waitingFor")).toString();
        if (!wf.isEmpty() && wf != QStringLiteral("none"))
            m.currentStep = wf;
        return m;
    }

    // Idle — nothing to show, skip JSONL read
    if (status == QStringLiteral("idle"))
        return m;

    // Transcript file — tail 8192 bytes
    const QString usedCwd = sobj.value(QStringLiteral("cwd")).toString();
    const QString enc = encodeCwd(usedCwd.isEmpty() ? cwd : usedCwd);
    const QString txPath =
        QStringLiteral("%1/.claude/projects/%2/%3.jsonl")
            .arg(home, enc, m.sessionId);

    QFile tf(txPath);
    if (!tf.open(QIODevice::ReadOnly))
        return m;
    if (watchPaths) *watchPaths << txPath;

    const qint64 size = tf.size();
    const qint64 tailSize = 8192;
    if (size > tailSize)
        tf.seek(size - tailSize);

    QByteArray tail = tf.readAll();
    // Discard first (potentially partial) line if we seeked
    if (size > tailSize) {
        int nl = tail.indexOf('\n');
        if (nl >= 0) tail = tail.mid(nl + 1);
    }

    m.currentStep = lastToolUse(tail);
    return m;
}

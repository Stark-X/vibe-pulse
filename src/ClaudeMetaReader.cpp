#include "ClaudeMetaReader.h"
#include "MiniMd.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

static constexpr int kClaudeContextLimit = 200000;

namespace {
struct ContextUsage { int used = 0; int limit = 0; };

ContextUsage parseContextUsage(const QByteArray &tail)
{
    const QList<QByteArray> lines = tail.split('\n');
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QByteArray raw = lines[i].trimmed();
        if (raw.isEmpty())
            continue;
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (err.error != QJsonParseError::NoError)
            continue;
        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("assistant"))
            continue;
        const QJsonObject usage = obj.value(QStringLiteral("message"))
                                     .toObject()
                                     .value(QStringLiteral("usage"))
                                     .toObject();
        if (usage.isEmpty())
            continue;
        const qint64 used = static_cast<qint64>(usage.value(QStringLiteral("input_tokens")).toInt())
                          + static_cast<qint64>(usage.value(QStringLiteral("cache_creation_input_tokens")).toInt())
                          + static_cast<qint64>(usage.value(QStringLiteral("cache_read_input_tokens")).toInt());
        return { static_cast<int>(qMin(used, static_cast<qint64>(kClaudeContextLimit))),
                 kClaudeContextLimit };
    }
    return {};
}
} // namespace

static QString encodeCwd(const QString &cwd)
{
    // Claude Code encodes cwd with: cwd.replace(/[^a-zA-Z0-9]/g, "-")
    // Every non-alphanumeric character (/, \, :, space, _, ., etc.) → "-".
    // Source: github.com/anthropics/claude-code/issues/24579 (encoding function Zz)
    QString enc;
    enc.reserve(cwd.size());
    for (const QChar ch : cwd)
        enc.append(ch.isLetterOrNumber() ? ch : QLatin1Char('-'));
    return enc;
}

static QString transcriptPath(const QString &home, const QString &cwd,
                               const QString &sessionId)
{
    return QStringLiteral("%1/.claude/projects/%2/%3.jsonl")
        .arg(home, encodeCwd(cwd), sessionId);
}

static QByteArray readTail(const QString &path, qint64 maxBytes,
                            QStringList *watchPaths)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    if (watchPaths) *watchPaths << path;

    const qint64 size = f.size();
    if (size > maxBytes)
        f.seek(size - maxBytes);

    QByteArray tail = f.readAll();
    if (size > maxBytes) {
        const int nl = tail.indexOf('\n');
        if (nl >= 0)
            tail = tail.mid(nl + 1);
    }
    return tail;
}

struct Interaction {
    enum Kind { None, Question, Plan, Permission };
    QString     id;
    QString     prompt;
    QString     title;
    QString     markdown;
    QString     tool;
    QString     target;
    QString     desc;
    QStringList options;
    Kind        kind = None;
};

static QStringList optionStrings(const QJsonArray &arr)
{
    QStringList result;
    for (const QJsonValue &v : arr) {
        if (v.isString()) {
            result << v.toString();
        } else {
            const QString label = v.toObject().value(QStringLiteral("label")).toString();
            if (!label.isEmpty())
                result << label;
        }
    }
    return result;
}

static Interaction lastClaudeInteraction(const QByteArray &tail)
{
    const QList<QByteArray> lines = tail.split('\n');
    for (int i = lines.size() - 1; i >= 0; --i) {
        const QByteArray raw = lines[i].trimmed();
        if (raw.isEmpty())
            continue;

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (err.error != QJsonParseError::NoError)
            continue;

        const QJsonObject obj = doc.object();
        if (obj.value(QStringLiteral("type")).toString() != QStringLiteral("assistant"))
            continue;

        const QJsonArray content = obj.value(QStringLiteral("message"))
                                       .toObject()
                                       .value(QStringLiteral("content"))
                                       .toArray();
        for (int j = content.size() - 1; j >= 0; --j) {
            const QJsonObject item = content[j].toObject();
            if (item.value(QStringLiteral("type")).toString() != QStringLiteral("tool_use"))
                continue;

            const QString name  = item.value(QStringLiteral("name")).toString();
            const QJsonObject in = item.value(QStringLiteral("input")).toObject();
            Interaction ix;
            ix.id = item.value(QStringLiteral("id")).toString();

            if (name == QStringLiteral("AskUserQuestion")) {
                ix.kind   = Interaction::Question;
                ix.prompt = in.value(QStringLiteral("question")).toString();
                if (ix.prompt.isEmpty())
                    ix.prompt = in.value(QStringLiteral("prompt")).toString();
                ix.options = optionStrings(in.value(QStringLiteral("options")).toArray());
                return ix;
            }
            if (name == QStringLiteral("ExitPlanMode")) {
                ix.kind     = Interaction::Plan;
                ix.markdown = in.value(QStringLiteral("plan")).toString();
                if (ix.markdown.isEmpty())
                    ix.markdown = in.value(QStringLiteral("markdown")).toString();
                ix.title = in.value(QStringLiteral("title")).toString();
                return ix;
            }
            if (name == QStringLiteral("Edit") || name == QStringLiteral("Write")
                    || name == QStringLiteral("MultiEdit")) {
                ix.kind   = Interaction::Permission;
                ix.tool   = name;
                ix.target = in.value(QStringLiteral("file_path")).toString();
                ix.desc   = in.value(QStringLiteral("description")).toString();
                return ix;
            }
            if (name == QStringLiteral("Bash")) {
                ix.kind   = Interaction::Permission;
                ix.tool   = name;
                ix.target = in.value(QStringLiteral("command")).toString();
                ix.desc   = in.value(QStringLiteral("description")).toString();
                return ix;
            }
            return {};
        }
    }
    return {};
}

static QString lastToolUse(const QByteArray &tail)
{
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
                detail = detail.section(QLatin1Char('\n'), 0, 0);
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

    QString sessionPath =
        QStringLiteral("%1/.claude/sessions/%2.json").arg(home).arg(pid);

    if (!QFileInfo::exists(sessionPath)) {
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

    if (status == QStringLiteral("waiting")) {
        const QString wf = sobj.value(QStringLiteral("waitingFor")).toString();
        if (!wf.isEmpty() && wf != QStringLiteral("none"))
            m.currentStep = wf;

        const QString usedCwd = m.cwd.isEmpty() ? cwd : m.cwd;
        const QByteArray tail = readTail(transcriptPath(home, usedCwd, m.sessionId),
                                         32768, watchPaths);
        const ContextUsage cu = parseContextUsage(tail);
        m.contextUsed  = cu.used;
        m.contextLimit = cu.limit;
        const Interaction ix = lastClaudeInteraction(tail);

        if (ix.kind == Interaction::Question) {
            m.pulseState     = PulseState::Question;
            m.questionPrompt = ix.prompt;
            m.questionOptions= ix.options;
            m.interactionId  = ix.id;
            return m;
        }
        if (ix.kind == Interaction::Plan) {
            m.pulseState   = PulseState::Plan;
            m.planTitle    = ix.title;
            m.planMarkdown = ix.markdown;
            m.planHtml     = miniMdToHtml(ix.markdown);
            m.interactionId= ix.id;
            return m;
        }
        if (ix.kind == Interaction::Permission) {
            m.pulseState       = PulseState::Permission;
            m.permissionTool   = ix.tool;
            m.permissionTarget = ix.target;
            m.permissionDesc   = ix.desc;
            m.interactionId    = ix.id;
            return m;
        }

        m.pulseState       = PulseState::Permission;
        m.permissionTarget = m.currentStep;
        return m;
    }

    if (status == QStringLiteral("idle")) {
        m.pulseState = PulseState::Idle;
        return m;
    }

    const QString usedCwd = sobj.value(QStringLiteral("cwd")).toString();
    const QString txPath = transcriptPath(home, usedCwd.isEmpty() ? cwd : usedCwd, m.sessionId);
    const QByteArray tail = readTail(txPath, 32768, watchPaths);
    const ContextUsage cu = parseContextUsage(tail);
    m.contextUsed  = cu.used;
    m.contextLimit = cu.limit;
    m.currentStep = lastToolUse(tail);
    m.pulseState  = PulseState::Working;
    return m;
}

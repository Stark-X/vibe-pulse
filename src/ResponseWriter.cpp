#include "ResponseWriter.h"

#include <unistd.h>

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

static QString safeSegment(const QString &v)
{
    QString r;
    r.reserve(v.size());
    for (const QChar ch : v) {
        if (ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_'))
            r.append(ch);
        else
            r.append(QLatin1Char('_'));
    }
    // Reject path traversal sequences
    if (r.isEmpty() || r == QStringLiteral(".") || r == QStringLiteral(".."))
        return {};
    return r;
}

static QString responsePath(const QString &sessionId, const QString &interactionId)
{
    const QString sid = safeSegment(sessionId);
    const QString iid = safeSegment(interactionId);
    if (sid.isEmpty() || iid.isEmpty())
        return {};

    const QByteArray runtime = qgetenv("XDG_RUNTIME_DIR");
    const QString base = runtime.isEmpty()
        ? QStringLiteral("/tmp/pulse-%1").arg(static_cast<uint>(getuid()))
        : QString::fromLocal8Bit(runtime) + QStringLiteral("/pulse");

    const QString dir = base + QLatin1Char('/') + sid;
    if (!QDir().mkpath(dir))
        return {};

    return dir + QLatin1Char('/') + iid + QStringLiteral(".response.json");
}

static bool writePayload(const QString &sessionId, const QString &interactionId,
                         QJsonObject payload)
{
    const QString path = responsePath(sessionId, interactionId);
    if (path.isEmpty())
        return false;

    payload.insert(QStringLiteral("sessionId"),     sessionId);
    payload.insert(QStringLiteral("interactionId"), interactionId);
    payload.insert(QStringLiteral("createdAt"),
                   QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return false;

    const QByteArray data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (f.write(data) != data.size())
        return false;

    return f.commit();
}

bool ResponseWriter::writeQuestionAnswer(const QString &sessionId,
                                          const QString &interactionId,
                                          int optionIndex,
                                          const QString &answer)
{
    QJsonObject p;
    p.insert(QStringLiteral("type"),        QStringLiteral("question_answer"));
    p.insert(QStringLiteral("optionIndex"), optionIndex);
    p.insert(QStringLiteral("answer"),      answer);
    return writePayload(sessionId, interactionId, p);
}

bool ResponseWriter::writePlanDecision(const QString &sessionId,
                                        const QString &interactionId,
                                        const QString &decision,
                                        const QString &comment)
{
    QJsonObject p;
    p.insert(QStringLiteral("type"),     QStringLiteral("plan_decision"));
    p.insert(QStringLiteral("decision"), decision);
    if (!comment.isEmpty())
        p.insert(QStringLiteral("comment"), comment);
    return writePayload(sessionId, interactionId, p);
}

bool ResponseWriter::writePermissionDecision(const QString &sessionId,
                                              const QString &interactionId,
                                              bool allow,
                                              const QString &amendment)
{
    QJsonObject p;
    p.insert(QStringLiteral("type"),     QStringLiteral("permission_decision"));
    p.insert(QStringLiteral("decision"), allow ? QStringLiteral("allow") : QStringLiteral("deny"));
    if (!amendment.isEmpty())
        p.insert(QStringLiteral("amendment"), amendment);
    return writePayload(sessionId, interactionId, p);
}

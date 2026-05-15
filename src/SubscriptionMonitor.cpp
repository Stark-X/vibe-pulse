#include "SubscriptionMonitor.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

static constexpr int kStartDelayMs   = 5000;
static constexpr int kPollIntervalMs = 300000;
static constexpr int kTimeoutMs      = 10000;

SubscriptionMonitor::SubscriptionMonitor(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(kPollIntervalMs);
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout, this, &SubscriptionMonitor::refreshAll);
}

void SubscriptionMonitor::start()
{
    if (qEnvironmentVariableIsSet("PULSE_MOCK_SUBSCRIPTION")) {
        parseMockEnv();
        return;
    }
    QTimer::singleShot(kStartDelayMs, this, &SubscriptionMonitor::refreshAll);
    m_pollTimer->start();
}

void SubscriptionMonitor::parseMockEnv()
{
    const QString env = QString::fromUtf8(qgetenv("PULSE_MOCK_SUBSCRIPTION"));
    for (const QString &part : env.split(QLatin1Char(','))) {
        const QStringList kv = part.trimmed().split(QLatin1Char('='));
        if (kv.size() != 2) continue;
        const QString key = kv[0].trimmed().toUpper();
        bool ok = false;
        const double val = qBound(0.0, kv[1].trimmed().toDouble(&ok), 100.0);
        if (!ok) continue;
        if (key == QStringLiteral("CC"))
            setClaudeState(true, val);
        else if (key == QStringLiteral("CD"))
            setCodexState(true, val);
    }
}

void SubscriptionMonitor::setClaudeState(bool ok, double util)
{
    if (m_claudeOk == ok && m_claudeUtil == util) return;
    m_claudeOk   = ok;
    m_claudeUtil = ok ? util : 0.0;
    emit dataChanged();
}

void SubscriptionMonitor::setCodexState(bool ok, double util)
{
    if (m_codexOk == ok && m_codexUtil == util) return;
    m_codexOk   = ok;
    m_codexUtil = ok ? util : 0.0;
    emit dataChanged();
}

void SubscriptionMonitor::refreshAll()
{
    refreshClaude();
    refreshCodex();
}

// ── Claude ──────────────────────────────────────────────────────────────────

bool SubscriptionMonitor::loadClaudeCredential(QString &outToken) const
{
    const QString path = QDir::homePath() + QStringLiteral("/.claude/.credentials.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject oauth = doc.object()
        .value(QStringLiteral("claudeAiOauth")).toObject();
    const QString token = oauth.value(QStringLiteral("accessToken")).toString();
    if (token.isEmpty())
        return false;

    const QJsonValue expiresAt = oauth.value(QStringLiteral("expiresAt"));
    if (!expiresAt.isUndefined() && !expiresAt.isNull()) {
        const qint64 expMs = static_cast<qint64>(expiresAt.toDouble());
        if (expMs > 0 && QDateTime::currentMSecsSinceEpoch() > expMs - 60000)
            return false;
    }

    outToken = token;
    return true;
}

void SubscriptionMonitor::refreshClaude()
{
    if (m_claudeReply) return;

    QString token;
    if (!loadClaudeCredential(token)) {
        setClaudeState(false);
        return;
    }

    QNetworkRequest req(QUrl(QStringLiteral("https://api.anthropic.com/api/oauth/usage")));
    req.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token.toUtf8());
    req.setRawHeader("anthropic-beta", "oauth-2025-04-20");
    req.setRawHeader("Accept", "application/json");
    req.setTransferTimeout(kTimeoutMs);

    QNetworkReply *reply = m_nam->get(req);
    m_claudeReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_claudeReply == reply) m_claudeReply = nullptr;
        handleClaudeReply(reply);
        reply->deleteLater();
    });
}

void SubscriptionMonitor::handleClaudeReply(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status >= 300) {
        setClaudeState(false);
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        setClaudeState(false);
        return;
    }

    const QJsonObject body = doc.object();

    auto extractUtil = [&](const QString &key) -> double {
        const QJsonObject w = body.value(key).toObject();
        if (w.isEmpty()) return -1.0;
        const double v = w.value(QStringLiteral("utilization")).toDouble(-1.0);
        return v;
    };

    double util = extractUtil(QStringLiteral("five_hour"));
    if (util < 0.0)
        util = extractUtil(QStringLiteral("seven_day"));

    if (util < 0.0)
        setClaudeState(false);
    else
        setClaudeState(true, qBound(0.0, util, 100.0));
}

// ── Codex ───────────────────────────────────────────────────────────────────

bool SubscriptionMonitor::loadCodexCredential(QString &outToken, QString &outAccountId) const
{
    const QString path = QDir::homePath() + QStringLiteral("/.codex/auth.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return false;

    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("auth_mode")).toString() != QStringLiteral("chatgpt"))
        return false;

    const QJsonObject tokens = obj.value(QStringLiteral("tokens")).toObject();
    const QString token = tokens.value(QStringLiteral("access_token")).toString();
    if (token.isEmpty())
        return false;

    outToken     = token;
    outAccountId = tokens.value(QStringLiteral("account_id")).toString();
    return true;
}

void SubscriptionMonitor::refreshCodex()
{
    if (m_codexReply) return;

    QString token, accountId;
    if (!loadCodexCredential(token, accountId)) {
        setCodexState(false);
        return;
    }

    QNetworkRequest req(QUrl(QStringLiteral("https://chatgpt.com/backend-api/wham/usage")));
    req.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token.toUtf8());
    req.setRawHeader("User-Agent", "codex-cli");
    req.setRawHeader("Accept", "application/json");
    if (!accountId.isEmpty())
        req.setRawHeader("ChatGPT-Account-Id", accountId.toUtf8());
    req.setTransferTimeout(kTimeoutMs);

    QNetworkReply *reply = m_nam->get(req);
    m_codexReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_codexReply == reply) m_codexReply = nullptr;
        handleCodexReply(reply);
        reply->deleteLater();
    });
}

void SubscriptionMonitor::handleCodexReply(QNetworkReply *reply)
{
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status >= 300) {
        setCodexState(false);
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        setCodexState(false);
        return;
    }

    const QJsonObject body = doc.object();
    const double val = body
        .value(QStringLiteral("rate_limit")).toObject()
        .value(QStringLiteral("primary_window")).toObject()
        .value(QStringLiteral("used_percent")).toDouble(-1.0);

    if (val < 0.0)
        setCodexState(false);
    else
        setCodexState(true, qBound(0.0, val, 100.0));
}

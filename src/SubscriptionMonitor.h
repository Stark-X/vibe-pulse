#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QTimer>

class SubscriptionMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double claudeUtilization READ claudeUtilization NOTIFY dataChanged)
    Q_PROPERTY(double codexUtilization  READ codexUtilization  NOTIFY dataChanged)
    Q_PROPERTY(bool   claudeAvailable   READ claudeAvailable   NOTIFY dataChanged)
    Q_PROPERTY(bool   codexAvailable    READ codexAvailable    NOTIFY dataChanged)

public:
    explicit SubscriptionMonitor(QObject *parent = nullptr);

    double claudeUtilization() const { return m_claudeUtil; }
    double codexUtilization()  const { return m_codexUtil;  }
    bool   claudeAvailable()   const { return m_claudeOk;   }
    bool   codexAvailable()    const { return m_codexOk;    }

    void start();

signals:
    void dataChanged();

private:
    void parseMockEnv();
    void refreshAll();
    void refreshClaude();
    void refreshCodex();
    void handleClaudeReply(QNetworkReply *reply);
    void handleCodexReply(QNetworkReply *reply);

    bool loadClaudeCredential(QString &outToken) const;
    bool loadCodexCredential(QString &outToken, QString &outAccountId) const;

    void setClaudeState(bool ok, double util = 0.0);
    void setCodexState(bool ok, double util = 0.0);

    double m_claudeUtil = 0.0;
    double m_codexUtil  = 0.0;
    bool   m_claudeOk   = false;
    bool   m_codexOk    = false;

    QNetworkAccessManager *m_nam         = nullptr;
    QNetworkReply         *m_claudeReply = nullptr;
    QNetworkReply         *m_codexReply  = nullptr;
    QTimer                *m_pollTimer   = nullptr;
};

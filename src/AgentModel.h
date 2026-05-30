#pragma once
#include "AgentInfo.h"
#include "WindowManager.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QTimer>
#include <QVector>

class AgentModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int     count          READ rowCount       NOTIFY countChanged)
    Q_PROPERTY(int     workingCount   READ workingCount   NOTIFY globalStateChanged)
    Q_PROPERTY(QString globalState    READ globalState    NOTIFY globalStateChanged)
    Q_PROPERTY(int     activeAgentRow READ activeAgentRow NOTIFY globalStateChanged)
    Q_PROPERTY(bool    idleCollapsed  READ idleCollapsed  NOTIFY globalStateChanged)

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        ToolTypeRole,
        PidRole,
        StatusRole,
        CwdRole,
        SessionIdRole,
        SessionNameRole,
        SessionBusyRole,
        CurrentStepRole,
        WindowAddressRole,
        CanJumpRole,
        PulseStateRole,
        QuestionPromptRole,
        QuestionOptionsRole,
        PlanTitleRole,
        PlanHtmlRole,
        PermissionToolRole,
        PermissionTargetRole,
        PermissionDescRole,
        InteractionIdRole,
        ContextUsedRole,
        ContextLimitRole,
    };
    Q_ENUM(Roles)

    explicit AgentModel(WindowManager *wm, QObject *parent = nullptr);

    int      rowCount(const QModelIndex & = {}) const override;
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString globalState()    const;
    int     workingCount()   const;
    int     activeAgentRow() const;
    bool    idleCollapsed()  const;

    Q_INVOKABLE QVariantMap get(int row) const;

    Q_INVOKABLE bool answerQuestion   (int row, int optionIndex);
    Q_INVOKABLE bool approvePlan      (int row);
    Q_INVOKABLE bool commentPlan      (int row, const QString &comment);
    Q_INVOKABLE bool decidePermission      (int row, bool allow);
    Q_INVOKABLE bool decidePermissionAmend (int row, bool allow, const QString &amendment);

public slots:
    void setSnapshot(QVector<AgentInfo> snapshot);
    void focusAgent(int index);

signals:
    void countChanged();
    void globalStateChanged();

private:
    void recomputeGlobalState();

    WindowManager      *m_wm = nullptr;
    QVector<AgentInfo>  m_agents;
    QString       m_globalState    = QStringLiteral("idle");
    int           m_activeAgentRow = -1;
    bool          m_idleCollapsed  = false;
    int           m_idleDelayMs    = 2500;
    QElapsedTimer m_idleSince;
    QTimer        m_idleCollapseTimer;
};

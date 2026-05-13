#pragma once
#include "AgentInfo.h"
#include <QAbstractListModel>
#include <QVector>

class AgentModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

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
    };
    Q_ENUM(Roles)

    explicit AgentModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex & = {}) const override;
    QVariant data(const QModelIndex &idx, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

public slots:
    void setSnapshot(QVector<AgentInfo> snapshot);
    void focusAgent(int index);

signals:
    void countChanged();

private:
    QVector<AgentInfo> m_agents;
};

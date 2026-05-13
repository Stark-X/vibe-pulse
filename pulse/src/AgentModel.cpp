#include "AgentModel.h"
#include "HyprlandClient.h"

AgentModel::AgentModel(QObject *parent) : QAbstractListModel(parent) {}

int AgentModel::rowCount(const QModelIndex &) const
{
    return m_agents.size();
}

QHash<int, QByteArray> AgentModel::roleNames() const
{
    return {
        { NameRole,         "name"         },
        { ToolTypeRole,     "toolType"     },
        { PidRole,          "pid"          },
        { StatusRole,       "status"       },
        { CwdRole,          "cwd"          },
        { SessionIdRole,    "sessionId"    },
        { SessionNameRole,  "sessionName"  },
        { SessionBusyRole,  "sessionBusy"  },
        { CurrentStepRole,  "currentStep"  },
        { WindowAddressRole,"windowAddress"},
        { CanJumpRole,      "canJump"      },
    };
}

QVariant AgentModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_agents.size())
        return {};

    const AgentInfo &a = m_agents[idx.row()];
    switch (role) {
    case NameRole:          return a.name;
    case ToolTypeRole:      return a.toolType;
    case PidRole:           return a.pid;
    case StatusRole:        return a.status;
    case CwdRole:           return a.cwd;
    case SessionIdRole:     return a.sessionId;
    case SessionNameRole:   return a.sessionName;
    case SessionBusyRole:   return a.sessionBusy;
    case CurrentStepRole:   return a.currentStep;
    case WindowAddressRole: return a.windowAddress;
    case CanJumpRole:       return !a.windowAddress.isEmpty();
    default:                return {};
    }
}

void AgentModel::setSnapshot(QVector<AgentInfo> snapshot)
{
    // Incremental update: identify inserts, removes, changes
    QVector<quint32> oldPids, newPids;
    for (const auto &a : m_agents) oldPids << a.pid;
    for (const auto &a : snapshot)  newPids << a.pid;

    // Removals (back to front to keep indices stable)
    for (int i = m_agents.size() - 1; i >= 0; --i) {
        if (!newPids.contains(m_agents[i].pid)) {
            beginRemoveRows({}, i, i);
            m_agents.remove(i);
            endRemoveRows();
        }
    }

    // Inserts and updates
    for (int ni = 0; ni < snapshot.size(); ++ni) {
        const AgentInfo &na = snapshot[ni];
        int oi = -1;
        for (int k = 0; k < m_agents.size(); ++k)
            if (m_agents[k].pid == na.pid && m_agents[k].toolType == na.toolType)
                { oi = k; break; }

        if (oi < 0) {
            // insert at end (already sorted by caller)
            int row = m_agents.size();
            beginInsertRows({}, row, row);
            m_agents.append(na);
            endInsertRows();
        } else {
            if (!m_agents[oi].dataEquals(na)) {
                m_agents[oi] = na;
                QModelIndex idx = index(oi);
                emit dataChanged(idx, idx);
            }
        }
    }

    emit countChanged();
}

void AgentModel::focusAgent(int row)
{
    if (row < 0 || row >= m_agents.size())
        return;
    const QString addr = m_agents[row].windowAddress;
    if (!addr.isEmpty())
        HyprlandClient::focusWindow(addr);
}

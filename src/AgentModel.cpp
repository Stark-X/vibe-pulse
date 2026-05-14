#include "AgentModel.h"
#include "HyprlandClient.h"
#include "ResponseWriter.h"

AgentModel::AgentModel(QObject *parent) : QAbstractListModel(parent)
{
    m_idleCollapseTimer.setSingleShot(true);
    m_idleCollapseTimer.setInterval(m_idleDelayMs);
    QObject::connect(&m_idleCollapseTimer, &QTimer::timeout, this, [this]() {
        recomputeGlobalState();
    });
}

static int statePriority(PulseState s)
{
    switch (s) {
    case PulseState::Permission: return 50;
    case PulseState::Question:   return 40;
    case PulseState::Plan:       return 30;
    case PulseState::Working:    return 20;
    case PulseState::Idle:       return 10;
    }
    return 0;
}

static QString stateName(PulseState s)
{
    switch (s) {
    case PulseState::Permission: return QStringLiteral("permission");
    case PulseState::Question:   return QStringLiteral("question");
    case PulseState::Plan:       return QStringLiteral("plan");
    case PulseState::Working:    return QStringLiteral("working");
    case PulseState::Idle:       return QStringLiteral("idle");
    }
    return QStringLiteral("idle");
}

int AgentModel::rowCount(const QModelIndex &) const
{
    return m_agents.size();
}

QHash<int, QByteArray> AgentModel::roleNames() const
{
    return {
        { NameRole,            "name"            },
        { ToolTypeRole,        "toolType"        },
        { PidRole,             "pid"             },
        { StatusRole,          "status"          },
        { CwdRole,             "cwd"             },
        { SessionIdRole,       "sessionId"       },
        { SessionNameRole,     "sessionName"     },
        { SessionBusyRole,     "sessionBusy"     },
        { CurrentStepRole,     "currentStep"     },
        { WindowAddressRole,   "windowAddress"   },
        { CanJumpRole,         "canJump"         },
        { PulseStateRole,      "pulseState"      },
        { QuestionPromptRole,  "questionPrompt"  },
        { QuestionOptionsRole, "questionOptions" },
        { PlanTitleRole,       "planTitle"       },
        { PlanHtmlRole,        "planHtml"        },
        { PermissionToolRole,  "permissionTool"  },
        { PermissionTargetRole,"permissionTarget"},
        { InteractionIdRole,   "interactionId"   },
    };
}

QVariant AgentModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || idx.row() >= m_agents.size())
        return {};

    const AgentInfo &a = m_agents[idx.row()];
    switch (role) {
    case NameRole:            return a.name;
    case ToolTypeRole:        return a.toolType;
    case PidRole:             return a.pid;
    case StatusRole:          return a.status;
    case CwdRole:             return a.cwd;
    case SessionIdRole:       return a.sessionId;
    case SessionNameRole:     return a.sessionName;
    case SessionBusyRole:     return a.sessionBusy;
    case CurrentStepRole:     return a.currentStep;
    case WindowAddressRole:   return a.windowAddress;
    case CanJumpRole:         return !a.windowAddress.isEmpty();
    case PulseStateRole:      return static_cast<int>(a.pulseState);
    case QuestionPromptRole:  return a.questionPrompt;
    case QuestionOptionsRole: return a.questionOptions;
    case PlanTitleRole:       return a.planTitle;
    case PlanHtmlRole:        return a.planHtml;
    case PermissionToolRole:  return a.permissionTool;
    case PermissionTargetRole:return a.permissionTarget;
    case InteractionIdRole:   return a.interactionId;
    default:                  return {};
    }
}

QString AgentModel::globalState() const    { return m_globalState; }
int     AgentModel::activeAgentRow() const { return m_activeAgentRow; }
bool    AgentModel::idleCollapsed() const  { return m_idleCollapsed; }

QVariantMap AgentModel::get(int row) const
{
    if (row < 0 || row >= m_agents.size())
        return {};
    const AgentInfo &a = m_agents[row];
    return {
        { QStringLiteral("name"),            a.name             },
        { QStringLiteral("toolType"),        a.toolType         },
        { QStringLiteral("pid"),             a.pid              },
        { QStringLiteral("status"),          a.status           },
        { QStringLiteral("cwd"),             a.cwd              },
        { QStringLiteral("sessionId"),       a.sessionId        },
        { QStringLiteral("sessionName"),     a.sessionName      },
        { QStringLiteral("sessionBusy"),     a.sessionBusy      },
        { QStringLiteral("currentStep"),     a.currentStep      },
        { QStringLiteral("windowAddress"),   a.windowAddress    },
        { QStringLiteral("canJump"),         !a.windowAddress.isEmpty() },
        { QStringLiteral("pulseState"),      static_cast<int>(a.pulseState) },
        { QStringLiteral("questionPrompt"),  a.questionPrompt   },
        { QStringLiteral("questionOptions"), a.questionOptions  },
        { QStringLiteral("planTitle"),       a.planTitle        },
        { QStringLiteral("planHtml"),        a.planHtml         },
        { QStringLiteral("permissionTool"),  a.permissionTool   },
        { QStringLiteral("permissionTarget"),a.permissionTarget },
        { QStringLiteral("interactionId"),   a.interactionId    },
    };
}

void AgentModel::setSnapshot(QVector<AgentInfo> snapshot)
{
    QVector<quint32> oldPids, newPids;
    for (const auto &a : m_agents) oldPids << a.pid;
    for (const auto &a : snapshot)  newPids << a.pid;

    for (int i = m_agents.size() - 1; i >= 0; --i) {
        if (!newPids.contains(m_agents[i].pid)) {
            beginRemoveRows({}, i, i);
            m_agents.remove(i);
            endRemoveRows();
        }
    }

    for (int ni = 0; ni < snapshot.size(); ++ni) {
        const AgentInfo &na = snapshot[ni];
        int oi = -1;
        for (int k = 0; k < m_agents.size(); ++k)
            if (m_agents[k].pid == na.pid && m_agents[k].toolType == na.toolType)
                { oi = k; break; }

        if (oi < 0) {
            int row = m_agents.size();
            beginInsertRows({}, row, row);
            m_agents.append(na);
            endInsertRows();
        } else {
            if (!m_agents[oi].dataEquals(na)) {
                m_agents[oi] = na;
                QModelIndex midx = index(oi);
                emit dataChanged(midx, midx);
            }
        }
    }

    emit countChanged();
    recomputeGlobalState();
}

void AgentModel::focusAgent(int row)
{
    if (row < 0 || row >= m_agents.size())
        return;
    const QString addr = m_agents[row].windowAddress;
    if (!addr.isEmpty())
        HyprlandClient::focusWindow(addr);
}

void AgentModel::recomputeGlobalState()
{
    int nextRow   = -1;
    int bestPrio  = -1;
    PulseState nextState = PulseState::Idle;

    for (int i = 0; i < m_agents.size(); ++i) {
        const int prio = statePriority(m_agents[i].pulseState);
        if (prio > bestPrio) {
            bestPrio  = prio;
            nextState = m_agents[i].pulseState;
            nextRow   = i;
        }
    }

    if (nextState == PulseState::Idle)
        nextRow = -1;

    const QString nextGlobal = stateName(nextState);
    bool nextCollapsed = false;
    if (nextState == PulseState::Idle) {
        if (!m_idleSince.isValid()) {
            m_idleSince.start();
            m_idleCollapseTimer.start(m_idleDelayMs);
        }
        nextCollapsed = m_agents.isEmpty()
            || m_idleSince.elapsed() >= m_idleDelayMs;
    } else {
        m_idleSince.invalidate();
        m_idleCollapseTimer.stop();
    }

    const bool changed = m_globalState != nextGlobal
        || m_activeAgentRow != nextRow
        || m_idleCollapsed  != nextCollapsed;

    m_globalState    = nextGlobal;
    m_activeAgentRow = nextRow;
    m_idleCollapsed  = nextCollapsed;

    if (changed)
        emit globalStateChanged();
}

bool AgentModel::answerQuestion(int row, int optionIndex)
{
    if (row < 0 || row >= m_agents.size())
        return false;
    const AgentInfo &a = m_agents[row];
    if (optionIndex < 0 || optionIndex >= a.questionOptions.size())
        return false;
    return ResponseWriter::writeQuestionAnswer(
        a.sessionId, a.interactionId, optionIndex, a.questionOptions.at(optionIndex));
}

bool AgentModel::approvePlan(int row)
{
    if (row < 0 || row >= m_agents.size())
        return false;
    const AgentInfo &a = m_agents[row];
    return ResponseWriter::writePlanDecision(
        a.sessionId, a.interactionId, QStringLiteral("approve"));
}

bool AgentModel::commentPlan(int row, const QString &comment)
{
    if (row < 0 || row >= m_agents.size())
        return false;
    const AgentInfo &a = m_agents[row];
    return ResponseWriter::writePlanDecision(
        a.sessionId, a.interactionId, QStringLiteral("comment"), comment);
}

bool AgentModel::decidePermission(int row, bool allow)
{
    if (row < 0 || row >= m_agents.size())
        return false;
    const AgentInfo &a = m_agents[row];
    return ResponseWriter::writePermissionDecision(a.sessionId, a.interactionId, allow);
}

bool AgentModel::decidePermissionAmend(int row, bool allow, const QString &amendment)
{
    if (row < 0 || row >= m_agents.size())
        return false;
    const AgentInfo &a = m_agents[row];
    return ResponseWriter::writePermissionDecision(a.sessionId, a.interactionId, allow, amendment);
}

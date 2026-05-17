#include "ProcScanner.h"
#include "MiniMd.h"
#include "ProcessTree.h"

#include <QFileInfo>

#include <algorithm>

// ── match rules ───────────────────────────────────────────────────────────────

struct AgentRule {
    const char *toolType;
    const char *comm;
    const char *exeExclude;   // nullptr = no exclusion
    const char *argvExclude;  // nullptr = no exclusion
};

static const AgentRule kRules[] = {
    { "Claude Code", "claude",   nullptr,      " agents"     },
    { "Codex",       "codex",    "/usr/lib/",  "app-server"  },
    { "OpenCode",    "opencode", nullptr,      nullptr       },
};

// ── ProcScanner ───────────────────────────────────────────────────────────────

ProcScanner::ProcScanner(QObject *parent) : QObject(parent)
{
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        emit snapshotReady(scanAll());
    });
}

void ProcScanner::start()
{
    emit snapshotReady(scanAll());
    m_timer.start(2000);
}

void ProcScanner::stop()
{
    m_timer.stop();
}

// ── demo injection ────────────────────────────────────────────────────────────
// PULSE_DEMO=permission|question|plan|working|idle  → inject a fake agent

static QVector<AgentInfo> demoSnapshot()
{
    const QByteArray mode = qgetenv("PULSE_DEMO");
    if (mode.isEmpty())
        return {};

    AgentInfo a;
    a.toolType    = QStringLiteral("Claude Code");
    a.pid         = 99999;
    a.status      = QStringLiteral("running");
    a.name        = QStringLiteral("vibe-island-hyper");
    a.cwd         = QStringLiteral("/home/stark/workspace/personal/vibe-island-hyper");
    a.sessionId   = QStringLiteral("demo-session-id");
    a.sessionName = QStringLiteral("demo");
    a.sessionBusy = true;
    a.interactionId = QStringLiteral("demo-interaction-id");

    if (mode == "permission") {
        a.pulseState       = PulseState::Permission;
        a.permissionTool   = QStringLiteral("Edit");
        a.permissionTarget = QStringLiteral("src/main.cpp");
        a.currentStep      = QStringLiteral("Edit src/main.cpp");
    } else if (mode == "question") {
        a.pulseState      = PulseState::Question;
        a.questionPrompt  = QStringLiteral("Which approach should I use for the state machine?");
        a.questionOptions = { QStringLiteral("Use QStateMachine from Qt"),
                              QStringLiteral("Custom string-based state in QML"),
                              QStringLiteral("Enum in C++ backend, string to QML") };
        a.currentStep     = QStringLiteral("Asking user");
    } else if (mode == "plan") {
        a.pulseState    = PulseState::Plan;
        a.planTitle     = QStringLiteral("Refactor state machine");
        a.planMarkdown  = QStringLiteral(
            "## Plan: fix auth bug\n"
            "I'll harden the JWT middleware so missing/expired tokens fail loudly.\n"
            "- Add explicit `AuthError` for missing tokens\n"
            "- Validate expiry in `verify()` with a 30s skew\n"
            "- Update 4 callers to catch `AuthError`\n"
            "- Add unit tests for missing/expired/invalid cases\n\n"
            "~28 lines changed across 3 files. Estimated 4 min.");
        a.planHtml = miniMdToHtml(a.planMarkdown);
    } else if (mode == "working") {
        a.pulseState  = PulseState::Working;
        a.currentStep = QStringLiteral("Write · src/AgentModel.cpp");
    } else {
        a.pulseState  = PulseState::Idle;
        a.sessionBusy = false;
    }

    return { a };
}

// ── scan ──────────────────────────────────────────────────────────────────────

QVector<AgentInfo> ProcScanner::scanAll()
{
    const QVector<AgentInfo> demo = demoSnapshot();
    if (!demo.isEmpty())
        return demo;

    QVector<AgentInfo> result;

    for (quint32 pid : ProcessTree::listAll()) {
        const QString comm = ProcessTree::comm(pid);
        if (comm.isEmpty())
            continue;

        for (const auto &rule : kRules) {
            if (comm != QLatin1String(rule.comm))
                continue;

            const QString exe     = ProcessTree::exe(pid);
            const QString cwd     = ProcessTree::cwd(pid);
            const QString cmdline = ProcessTree::cmdline(pid).join(QLatin1Char(' '));

            if (rule.exeExclude && exe.contains(QLatin1String(rule.exeExclude)))
                continue;
            if (rule.argvExclude && cmdline.contains(QLatin1String(rule.argvExclude)))
                continue;

            AgentInfo a;
            a.toolType    = QString::fromLatin1(rule.toolType);
            a.pid         = pid;
            a.status      = QStringLiteral("running");
            a.cwd         = cwd;
            a.name        = cwd.isEmpty() ? QStringLiteral("?") : QFileInfo(cwd).fileName();
            a.sessionBusy = true;

            result.append(a);
            break;
        }
    }

    dedup(result);
    std::sort(result.begin(), result.end(),
              [](const AgentInfo &a, const AgentInfo &b) { return a.pid < b.pid; });
    return result;
}

// ── dedup ─────────────────────────────────────────────────────────────────────

static bool hasAncestorInSet(quint32 pid, const QVector<quint32> &pids, int depth = 0)
{
    if (depth > 16)
        return false;
    const quint32 parent = ProcessTree::ppid(pid);
    if (parent == 0 || parent == 1)
        return false;
    if (pids.contains(parent))
        return true;
    return hasAncestorInSet(parent, pids, depth + 1);
}

void ProcScanner::dedup(QVector<AgentInfo> &agents)
{
    QHash<QString, QVector<quint32>> byType;
    for (const auto &a : agents)
        byType[a.toolType].append(a.pid);

    QVector<quint32> toRemove;
    for (auto it = byType.begin(); it != byType.end(); ++it) {
        const QVector<quint32> &pids = it.value();
        if (pids.size() < 2)
            continue;
        for (quint32 pid : pids)
            if (hasAncestorInSet(pid, pids))
                toRemove.append(pid);
    }

    agents.erase(
        std::remove_if(agents.begin(), agents.end(),
                       [&](const AgentInfo &a) { return toRemove.contains(a.pid); }),
        agents.end());
}

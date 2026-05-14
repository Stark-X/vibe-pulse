#include "ProcScanner.h"
#include "MiniMd.h"

#include <QDir>
#include <QFile>
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
    emit snapshotReady(scanAll());   // immediate first scan
    m_timer.start(2000);
}

void ProcScanner::stop()
{
    m_timer.stop();
}

// ── procfs helpers ────────────────────────────────────────────────────────────

bool ProcScanner::readStatus(quint32 pid, quint32 &outTgid, quint32 &outPpid)
{
    QFile f(QStringLiteral("/proc/%1/status").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return false;

    // /proc virtual files report size=0, so atEnd() lies — use readAll()
    const QByteArray content = f.readAll();

    quint32 tgid = 0, ppid = 0, pidVal = 0;
    for (const QByteArray &line : content.split('\n')) {
        if (line.startsWith("Pid:"))
            pidVal = line.mid(4).trimmed().toUInt();
        else if (line.startsWith("Tgid:"))
            tgid = line.mid(5).trimmed().toUInt();
        else if (line.startsWith("PPid:"))
            ppid = line.mid(5).trimmed().toUInt();
    }

    if (pidVal != tgid || tgid == 0)
        return false;

    outTgid = tgid;
    outPpid = ppid;
    return true;
}

QString ProcScanner::readComm(quint32 pid)
{
    QFile f(QStringLiteral("/proc/%1/comm").arg(pid));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromLocal8Bit(f.readAll()).trimmed();
}

QString ProcScanner::readExe(quint32 pid)
{
    return QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).symLinkTarget();
}

QString ProcScanner::readCwd(quint32 pid)
{
    return QFileInfo(QStringLiteral("/proc/%1/cwd").arg(pid)).symLinkTarget();
}

QStringList ProcScanner::readCmdline(quint32 pid)
{
    QFile f(QStringLiteral("/proc/%1/cmdline").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QByteArray raw = f.read(4096);
    QStringList args;
    for (const auto &part : raw.split('\0'))
        if (!part.isEmpty())
            args << QString::fromLocal8Bit(part);
    return args;
}

quint32 ProcScanner::ppidOf(quint32 pid)
{
    quint32 tgid = 0, ppid = 0;
    readStatus(pid, tgid, ppid);
    return ppid;
}

// ── scan ──────────────────────────────────────────────────────────────────────

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

QVector<AgentInfo> ProcScanner::scanAll()
{
    const QVector<AgentInfo> demo = demoSnapshot();
    if (!demo.isEmpty())
        return demo;

    QVector<AgentInfo> result;

    const QDir procDir(QStringLiteral("/proc"));
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &entry : entries) {
        bool ok = false;
        quint32 pid = entry.toUInt(&ok);
        if (!ok)
            continue;

        quint32 tgid = 0, ppid = 0;
        if (!readStatus(pid, tgid, ppid))
            continue;

        QString comm = readComm(pid);
        if (comm.isEmpty())
            continue;

        for (const auto &rule : kRules) {
            if (comm != QLatin1String(rule.comm))
                continue;

            QString exe  = readExe(pid);
            QString cwd  = readCwd(pid);
            QStringList argv = readCmdline(pid);
            QString cmdline  = argv.join(QLatin1Char(' '));

            if (rule.exeExclude && exe.contains(QLatin1String(rule.exeExclude)))
                continue;
            if (rule.argvExclude && cmdline.contains(QLatin1String(rule.argvExclude)))
                continue;

            AgentInfo a;
            a.toolType   = QString::fromLatin1(rule.toolType);
            a.pid        = pid;
            a.status     = QStringLiteral("running");
            a.cwd        = cwd;
            a.name       = cwd.isEmpty() ? QStringLiteral("?") : QFileInfo(cwd).fileName();
            a.sessionBusy = true;

            result.append(a);
            break;
        }
    }

    dedup(result);
    std::sort(result.begin(), result.end(),
              [](const AgentInfo &a, const AgentInfo &b){ return a.pid < b.pid; });
    return result;
}

// ── dedup ─────────────────────────────────────────────────────────────────────

// Returns true if 'ancestor' PID appears somewhere in the PPid chain of 'pid'
// (same toolType only; max 16 hops)
static bool hasAncestorInSet(quint32 pid, const QVector<quint32> &pids, int depth = 0)
{
    if (depth > 16)
        return false;
    quint32 ppid = 0;
    QFile f(QStringLiteral("/proc/%1/status").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    for (const QByteArray &line : f.readAll().split('\n')) {
        if (line.startsWith("PPid:")) {
            ppid = line.mid(5).trimmed().toUInt();
            break;
        }
    }
    if (ppid == 0 || ppid == 1)
        return false;
    if (pids.contains(ppid))
        return true;
    return hasAncestorInSet(ppid, pids, depth + 1);
}

void ProcScanner::dedup(QVector<AgentInfo> &agents)
{
    // Group pids by toolType
    QHash<QString, QVector<quint32>> byType;
    for (const auto &a : agents)
        byType[a.toolType].append(a.pid);

    QVector<quint32> toRemove;
    for (auto it = byType.begin(); it != byType.end(); ++it) {
        const QVector<quint32> &pids = it.value();
        if (pids.size() < 2)
            continue;
        for (quint32 pid : pids) {
            if (hasAncestorInSet(pid, pids))
                toRemove.append(pid);
        }
    }

    agents.erase(
        std::remove_if(agents.begin(), agents.end(),
                       [&](const AgentInfo &a){ return toRemove.contains(a.pid); }),
        agents.end());
}

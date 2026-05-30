#include <signal.h>

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QTimer>

#include "AgentModel.h"
#include "ClaudeMetaReader.h"
#include "CodexMetaReader.h"
#include "ProcessTree.h"
#include "ProcScanner.h"
#include "Settings.h"
#include "SubscriptionMonitor.h"
#include "TmuxResolver.h"
#include "WindowManager.h"
#include "WindowOverlay.h"

// Build a mock snapshot for UI preview — PULSE_MOCK=<scenario>
// Scenarios: idle, working, permission, question, plan, expanded
static QVector<AgentInfo> buildMockSnapshot(const QString &scenario)
{
    QVector<AgentInfo> agents;

    static quint32 mockPidCounter = 10000;
    auto mkAgent = [](const QString &name, const QString &tool, const QString &step,
                      PulseState state, bool busy = true) -> AgentInfo {
        AgentInfo a;
        a.name        = name;
        a.toolType    = tool;
        a.pid         = mockPidCounter++;
        a.status      = "running";
        a.cwd         = "/home/user/" + name;
        a.sessionId   = "sess-abc";
        a.sessionName = name + "-session";
        a.sessionBusy = busy;
        a.currentStep = step;
        a.pulseState  = state;
        return a;
    };

    if (scenario == "idle") {
        agents << mkAgent("vibe-island", "Claude Code", "", PulseState::Idle, false);
        agents << mkAgent("nova-sdk",    "Codex",       "", PulseState::Idle, false);
        agents << mkAgent("deepbank",    "Claude Code", "", PulseState::Idle, false);
    } else if (scenario == "working") {
        AgentInfo a = mkAgent("vibe-island", "Claude Code",
                              "Reading src/AgentModel.cpp", PulseState::Working);
        agents << a;
    } else if (scenario == "permission") {
        AgentInfo a = mkAgent("vibe-island", "Claude Code", "", PulseState::Permission);
        a.permissionTool   = "Edit";
        a.permissionTarget = "src/AgentModel.cpp";
        a.interactionId    = "perm-001";
        agents << a;
    } else if (scenario == "question") {
        AgentInfo a = mkAgent("vibe-island", "Claude Code", "", PulseState::Question);
        a.questionPrompt  = "Which approach should I use for the cache invalidation?";
        a.questionOptions = { "Invalidate on write", "TTL-based expiry", "Manual flush only" };
        a.interactionId   = "q-001";
        agents << a;
    } else if (scenario == "plan") {
        AgentInfo a = mkAgent("vibe-island", "Claude Code", "", PulseState::Plan);
        a.planTitle    = "Refactor AgentModel";
        a.planMarkdown = "## Plan\n1. Extract state machine\n2. Add unit tests\n3. Update QML bindings";
        a.planHtml     = "<b>Step 1</b> — Extract state machine into <code>AgentStateMachine.h</code><br>"
                         "<b>Step 2</b> — Add unit tests for all transitions<br>"
                         "<b>Step 3</b> — Update QML bindings in <code>main.qml</code>";
        a.interactionId = "plan-001";
        agents << a;
    } else {
        // "expanded" — multiple agents
        AgentInfo a1 = mkAgent("vibe-island",  "Claude Code", "Writing unit tests",    PulseState::Working);
        a1.contextUsed  = 45000;
        a1.contextLimit = 200000;
        agents << a1;
        AgentInfo a2 = mkAgent("nova-sdk",     "Codex",       "Refactoring auth flow", PulseState::Working);
        agents << a2;
        AgentInfo a3 = mkAgent("deepbank-fe",  "Claude Code", "",                      PulseState::Idle, false);
        a3.contextUsed  = 155000;
        a3.contextLimit = 200000;
        agents << a3;
        agents << mkAgent("api-gateway",  "Codex",       "",                      PulseState::Idle, false);
        agents << mkAgent("frontend-ui", "Claude Code", "Running lint checks",   PulseState::Working);
    }

    return agents;
}

// Enrich agents in-place with meta (session info, current step, window address).
// Also returns JSONL/session paths that should be watched for live updates.
static QStringList enrichAgents(QVector<AgentInfo> &agents,
                                 WindowManager *wm,
                                 QFileSystemWatcher *watcher = nullptr)
{
    QStringList newPaths;

    for (auto &a : agents) {
        if (a.toolType == QStringLiteral("Claude Code")) {
            ClaudeMeta m = ClaudeMetaReader::read(a.pid, a.cwd, &newPaths);
            a.sessionId       = m.sessionId;
            a.sessionName     = m.sessionName;
            a.sessionBusy     = m.sessionBusy;
            a.currentStep     = m.currentStep;
            a.pulseState      = m.pulseState;
            a.questionPrompt  = m.questionPrompt;
            a.questionOptions = m.questionOptions;
            a.planTitle       = m.planTitle;
            a.planMarkdown    = m.planMarkdown;
            a.planHtml        = m.planHtml;
            a.permissionTool  = m.permissionTool;
            a.permissionTarget= m.permissionTarget;
            a.permissionDesc  = m.permissionDesc;
            a.interactionId   = m.interactionId;
            if (m.contextUsed > 0) {
                a.contextUsed  = m.contextUsed;
                a.contextLimit = m.contextLimit;
            }
            if (!m.cwd.isEmpty()) {
                a.cwd  = m.cwd;
                a.name = QFileInfo(m.cwd).fileName();
            }
        }
        if (a.toolType == QStringLiteral("Codex")) {
            CodexMeta cm = CodexMetaReader::read(a.pid, &newPaths);
            a.sessionId    = cm.sessionId;
            a.sessionName  = cm.sessionName;
            a.sessionBusy  = cm.sessionBusy;
            a.currentStep  = cm.currentStep;
            a.pulseState   = cm.pulseState;
            if (cm.contextUsed > 0) {
                a.contextUsed  = cm.contextUsed;
                a.contextLimit = cm.contextLimit;
            }
        }
        a.windowAddress = wm->findWindowByPid(a.pid);
        if (a.windowAddress.isEmpty()) {
            if (auto pi = TmuxResolver::findPaneInfo(a.pid)) {
                // tmux client PID is a CLI process; walk up the ancestor chain to
                // find the GUI terminal emulator (iTerm2, Terminal.app, etc.)
                for (quint32 p : ProcessTree::ancestorChain(pi->terminalPid)) {
                    a.windowAddress = wm->findWindowByPid(p);
                    if (!a.windowAddress.isEmpty()) break;
                }
                a.tmuxTarget    = pi->tmuxTarget;
                a.tmuxClientTty = pi->clientTty;
                // Fallback: ancestor chain from tmux client PID may not reach the
                // GUI terminal if the client was reparented. Try via TTY device instead.
                if (a.windowAddress.isEmpty() && !a.tmuxClientTty.isEmpty())
                    a.windowAddress = wm->findWindowByTTY(a.tmuxClientTty);
            }
        }
    }

    if (watcher) {
        // Sync watcher: remove stale paths, but keep always-watched directories
        const QString sessionsDir =
            QDir::homePath() + QStringLiteral("/.claude/sessions");
        const QStringList current = watcher->files() + watcher->directories();
        for (const QString &p : current)
            if (!newPaths.contains(p) && p != sessionsDir)
                watcher->removePath(p);
        for (const QString &p : newPaths)
            if (!current.contains(p))
                watcher->addPath(p);
    }

    return newPaths;
}

int main(int argc, char **argv)
{
    qRegisterMetaType<AgentInfo>();
    qRegisterMetaType<QVector<AgentInfo>>();

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("pulse"));
    app.setOrganizationName(QStringLiteral("Pulse"));

    // Detect the running compositor and select the appropriate WM backend.
    // On Hyprland: uses hyprctl IPC for window discovery and resize.
    // On other compositors: no-op (layer-shell set_size handles resize).
    auto wm = WindowManager::create();
    WindowManager *wmPtr = wm.get();

    auto *model    = new AgentModel(wmPtr, &app);
    auto *settings = new Settings(&app);
    auto *scanner  = new ProcScanner(&app);
    auto *subMon   = new SubscriptionMonitor(&app);

    // ── live meta watcher ─────────────────────────────────────────────────────
    // Watches session files + active JSONL transcripts.
    // A 150 ms single-shot debounce avoids redundant re-reads when multiple
    // files change at once (e.g. session JSON + JSONL on the same turn).
    auto *watcher  = new QFileSystemWatcher(&app);
    auto *debounce = new QTimer(&app);
    debounce->setSingleShot(true);
    debounce->setInterval(150);

    // Always watch the sessions directory so we catch new session files
    watcher->addPath(QDir::homePath() + QStringLiteral("/.claude/sessions"));

    // Shared base agents (proc scan result before enrichment)
    auto *baseAgents = new QVector<AgentInfo>();
    const bool demoMode = !qgetenv("PULSE_DEMO").isEmpty();
    const QString mockScenario = QString::fromUtf8(qgetenv("PULSE_MOCK"));
    const bool isMock = !mockScenario.isEmpty();

    auto doRefresh = [=]() {
        if (baseAgents->isEmpty() || demoMode || isMock) return;
        QVector<AgentInfo> agents = *baseAgents;
        enrichAgents(agents, wmPtr, watcher);
        model->setSnapshot(agents);
    };

    QObject::connect(debounce, &QTimer::timeout, &app, doRefresh);
    QObject::connect(watcher, &QFileSystemWatcher::fileChanged,
                     debounce, [debounce](const QString &) { debounce->start(); });
    QObject::connect(watcher, &QFileSystemWatcher::directoryChanged,
                     debounce, [debounce](const QString &) { debounce->start(); });

    if (isMock) {
        // Inject static mock snapshot — scanner never starts
        model->setSnapshot(buildMockSnapshot(mockScenario));
    }

    // ── proc scanner ──────────────────────────────────────────────────────────
    QObject::connect(scanner, &ProcScanner::snapshotReady, &app,
                     [=](QVector<AgentInfo> agents) {
        if (isMock) return;  // mock mode: ignore real proc data
        *baseAgents = agents;
        if (!demoMode)
            enrichAgents(agents, wmPtr, watcher);  // skip enrichment in demo mode
        model->setSnapshot(agents);
    });

    // ── QML engine ────────────────────────────────────────────────────────────
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/"));
    engine.rootContext()->setContextProperty(QStringLiteral("agentModel"),  model);
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), settings);
    engine.rootContext()->setContextProperty(QStringLiteral("subscriptionMonitor"), subMon);
    engine.rootContext()->setContextProperty(QStringLiteral("mockForceExpanded"),
        QVariant(mockScenario == QStringLiteral("expanded")));

    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/main.qml")),
                             QQmlComponent::PreferSynchronous);
    if (component.isError()) {
        for (const auto &e : component.errors())
            qWarning().noquote() << e.toString();
        return 1;
    }

    QObject *root = component.beginCreate(engine.rootContext());
    auto *window  = qobject_cast<QWindow *>(root);
    if (!window) {
        qWarning("pulse: root must be a Window");
        delete root;
        return 1;
    }

    auto overlay = WindowOverlay::create();
    overlay->setup(window);

    component.completeCreate();
    subMon->start();
    window->show();

    // On compositors that need external resize IPC (currently Hyprland), use
    // the window manager's resizeWindow to keep the overlay at the correct size
    // and position. On others, layer-shell set_size is sufficient.
    if (wmPtr->needsExternalResize()) {
        auto doResize = [window, wmPtr](const QString &addr) {
            const qreal dpr = window->devicePixelRatio();
            const int   w   = qRound(window->width()  * dpr);
            const int   h   = qRound(qMin(window->height(), 600) * dpr);
            wmPtr->resizeWindow(addr, w, h);
        };

        auto *selfAddr  = new QString();
        auto *addrTimer = new QTimer(window);
        addrTimer->setSingleShot(false);
        addrTimer->setInterval(400);
        QObject::connect(addrTimer, &QTimer::timeout, window,
                         [selfAddr, addrTimer, doResize, wmPtr]() {
            if (!selfAddr->isEmpty()) { addrTimer->stop(); return; }
            *selfAddr = wmPtr->findWindowByPid(
                static_cast<quint32>(QCoreApplication::applicationPid()));
            if (!selfAddr->isEmpty())
                doResize(*selfAddr);
        });
        addrTimer->start();

        auto *resizeTimer = new QTimer(window);
        resizeTimer->setSingleShot(true);
        resizeTimer->setInterval(100);
        QObject::connect(window, &QWindow::widthChanged,  resizeTimer, [resizeTimer]{ resizeTimer->start(); });
        QObject::connect(window, &QWindow::heightChanged, resizeTimer, [resizeTimer]{ resizeTimer->start(); });
        QObject::connect(resizeTimer, &QTimer::timeout, window, [selfAddr, doResize]() {
            if (!selfAddr->isEmpty())
                doResize(*selfAddr);
        });
    }

    // ── IPC socket ────────────────────────────────────────────────────────────
    // QLocalServer maps to Unix socket on Unix, named pipe on Windows.
    const QString serverName = QStringLiteral("pulse-ipc");
    QLocalServer::removeServer(serverName);
    auto *server = new QLocalServer(&app);
    server->listen(serverName);
    QObject::connect(server, &QLocalServer::newConnection, window,
                     [server, window]() {
        QLocalSocket *conn = server->nextPendingConnection();
        QObject::connect(conn, &QLocalSocket::readyRead, window,
                         [conn, window]() {
            const QByteArray data = conn->readAll();
            QJsonObject obj = QJsonDocument::fromJson(data).object();
            const QString cmd = obj.value(QStringLiteral("cmd")).toString();
            if (cmd == QStringLiteral("toggle"))
                window->setVisible(!window->isVisible());
            else if (cmd == QStringLiteral("show"))
                window->setVisible(true);
            else if (cmd == QStringLiteral("hide"))
                window->setVisible(false);
            conn->close();
            conn->deleteLater();
        });
    });

    // ── graceful shutdown ──────────────────────────────────────────────────────
    QObject::connect(&app, &QGuiApplication::aboutToQuit, &app, [scanner, server, serverName]() {
        scanner->stop();
        server->close();
        QLocalServer::removeServer(serverName);
    });

    // SIGTERM/SIGINT → graceful quit (ensures aboutToQuit fires).
    // SIGTERM is POSIX-only; Windows only exposes SIGINT (Ctrl+C).
#ifndef Q_OS_WIN
    signal(SIGTERM, [](int) { QCoreApplication::quit(); });
#endif
    signal(SIGINT,  [](int) { QCoreApplication::quit(); });

    if (!isMock)
        scanner->start();
    return app.exec();
}

#include <unistd.h>

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
#include "HyprlandClient.h"
#include "ProcScanner.h"
#include "Settings.h"
#include "TmuxResolver.h"
#include "WaylandLayerShell.h"

// Enrich agents in-place with meta (session info, current step, window address).
// Also returns JSONL/session paths that should be watched for live updates.
static QStringList enrichAgents(QVector<AgentInfo> &agents,
                                 QFileSystemWatcher *watcher = nullptr)
{
    QStringList newPaths;
    const QVector<HyprWindow> wins =
        HyprlandClient::available() ? HyprlandClient::clients() : QVector<HyprWindow>{};

    for (auto &a : agents) {
        if (a.toolType == QStringLiteral("Claude Code")) {
            ClaudeMeta m = ClaudeMetaReader::read(a.pid, a.cwd, &newPaths);
            a.sessionId   = m.sessionId;
            a.sessionName = m.sessionName;
            a.sessionBusy = m.sessionBusy;
            a.currentStep = m.currentStep;
            if (!m.cwd.isEmpty()) {
                a.cwd  = m.cwd;
                a.name = QFileInfo(m.cwd).fileName();
            }
        }
        if (a.toolType == QStringLiteral("Codex")) {
            CodexMeta cm = CodexMetaReader::read(a.pid, &newPaths);
            a.sessionId   = cm.sessionId;
            a.sessionName = cm.sessionName;
            a.currentStep = cm.currentStep;
        }
        a.windowAddress = HyprlandClient::findWindowAddress(a.pid, wins);
        if (a.windowAddress.isEmpty()) {
            if (auto tp = TmuxResolver::findTerminalPid(a.pid))
                a.windowAddress = HyprlandClient::findWindowAddress(*tp, wins);
        }
    }

    if (watcher) {
        // Sync watcher: remove stale paths, add new ones
        const QStringList current = watcher->files() + watcher->directories();
        for (const QString &p : current)
            if (!newPaths.contains(p))
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

    auto *model    = new AgentModel(&app);
    auto *settings = new Settings(&app);
    auto *scanner  = new ProcScanner(&app);

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

    auto doRefresh = [=]() {
        if (baseAgents->isEmpty()) return;
        QVector<AgentInfo> agents = *baseAgents;
        enrichAgents(agents, watcher);
        model->setSnapshot(agents);
    };

    QObject::connect(debounce, &QTimer::timeout, &app, doRefresh);
    QObject::connect(watcher, &QFileSystemWatcher::fileChanged,
                     debounce, [debounce](const QString &) { debounce->start(); });
    QObject::connect(watcher, &QFileSystemWatcher::directoryChanged,
                     debounce, [debounce](const QString &) { debounce->start(); });

    // ── proc scanner ──────────────────────────────────────────────────────────
    QObject::connect(scanner, &ProcScanner::snapshotReady, &app,
                     [=](QVector<AgentInfo> agents) {
        *baseAgents = agents;           // save raw proc scan result
        enrichAgents(agents, watcher);  // enrich + update watch list
        model->setSnapshot(agents);
    });

    // ── QML engine ────────────────────────────────────────────────────────────
    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("agentModel"),  model);
    engine.rootContext()->setContextProperty(QStringLiteral("appSettings"), settings);

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

    WaylandLayerShell layerShell(window);
    if (!layerShell.isValid())
        qWarning("pulse: layer-shell unavailable, running as normal window");

    component.completeCreate();
    window->show();

    // After first frame, find our own window address and track height changes
    if (HyprlandClient::available()) {
        auto *selfAddr = new QString();
        QTimer::singleShot(500, window, [selfAddr, window]() {
            const auto wins = HyprlandClient::clients();
            *selfAddr = HyprlandClient::findWindowAddress(
                static_cast<quint32>(QCoreApplication::applicationPid()), wins);
        });
        auto *resizeTimer = new QTimer(window);
        resizeTimer->setSingleShot(true);
        resizeTimer->setInterval(50);
        QObject::connect(window, &QWindow::heightChanged, resizeTimer, [resizeTimer]() {
            resizeTimer->start();
        });
        QObject::connect(resizeTimer, &QTimer::timeout, window, [selfAddr, window]() {
            if (!selfAddr->isEmpty())
                HyprlandClient::resizeWindow(*selfAddr, window->width(), qMin(window->height(), 600));
        });
    }

    // ── IPC socket ────────────────────────────────────────────────────────────
    const QString socketPath =
        QStringLiteral("/tmp/pulse-%1.sock").arg(static_cast<uint>(getuid()));
    QLocalServer::removeServer(socketPath);
    auto *server = new QLocalServer(&app);
    server->listen(socketPath);
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

    scanner->start();
    return app.exec();
}

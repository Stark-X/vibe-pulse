#include "HyprlandClient.h"
#include "ProcessTree.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

bool HyprlandClient::available()
{
    return !qgetenv("HYPRLAND_INSTANCE_SIGNATURE").isEmpty();
}

QVector<HyprWindow> HyprlandClient::clients()
{
    if (!available())
        return {};

    QProcess p;
    p.start(QStringLiteral("hyprctl"), {QStringLiteral("clients"), QStringLiteral("-j")});
    if (!p.waitForFinished(1000)) { p.kill(); p.waitForFinished(100); return {}; }
    if (p.exitCode() != 0)
        return {};

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(p.readAllStandardOutput(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return {};

    QVector<HyprWindow> result;
    for (const QJsonValue &v : doc.array()) {
        QJsonObject o = v.toObject();
        HyprWindow w;
        w.address = o.value(QStringLiteral("address")).toString();
        w.pid     = o.value(QStringLiteral("pid")).toInteger();
        w.cls     = o.value(QStringLiteral("class")).toString();
        w.title   = o.value(QStringLiteral("title")).toString();
        const QJsonArray at = o.value(QStringLiteral("at")).toArray();
        w.x       = at.size() >= 2 ? at.at(0).toInt() : 0;
        w.y       = at.size() >= 2 ? at.at(1).toInt() : 0;
        if (!w.address.isEmpty())
            result.append(w);
    }
    return result;
}

QString HyprlandClient::focusWindow(const QString &address)
{
    if (address.isEmpty())
        return QStringLiteral("empty address");
    if (!available())
        return QStringLiteral("Hyprland not available");

    QProcess p;
    p.start(QStringLiteral("hyprctl"),
            {QStringLiteral("dispatch"), QStringLiteral("focuswindow"),
             QStringLiteral("address:") + address});
    if (!p.waitForFinished(1000)) { p.kill(); p.waitForFinished(100); return QStringLiteral("timeout"); }
    if (p.exitCode() != 0)
        return QString::fromLocal8Bit(p.readAllStandardError()).trimmed();
    return {};
}

void HyprlandClient::resizeWindow(const QString &address, int width, int height, int x, int y)
{
    if (address.isEmpty() || !available()) return;
    // Batch resize + move so the window stays at (x, y) after resize.
    // resizewindowpixel on floating windows scales from center, so the move
    // re-anchors the top-left corner back to the pre-resize position.
    const QString cmd =
        QStringLiteral("dispatch resizewindowpixel exact %1 %2,address:%3 ; "
                       "dispatch movewindowpixel exact %4 %5,address:%3")
        .arg(width).arg(height).arg(address).arg(x).arg(y);
    QProcess::startDetached(QStringLiteral("hyprctl"), {QStringLiteral("--batch"), cmd});
}

void HyprlandClient::resizeWindow(const QString &address, int width, int height)
{
    if (address.isEmpty() || !available()) return;
    const QString cmd = QStringLiteral("dispatch resizewindowpixel exact %1 %2,address:%3")
        .arg(width).arg(height).arg(address);
    QProcess::startDetached(QStringLiteral("hyprctl"), {QStringLiteral("--batch"), cmd});
}

QString HyprlandClient::findWindowAddress(quint32 agentPid,
                                           const QVector<HyprWindow> &wins)
{
    // direct match
    for (const auto &w : wins)
        if (static_cast<quint32>(w.pid) == agentPid)
            return w.address;

    // walk PPid chain (up to 32 hops)
    quint32 cur = agentPid;
    for (int i = 0; i < 32; ++i) {
        cur = ProcessTree::ppid(cur);
        if (cur == 0 || cur == 1)
            break;
        for (const auto &w : wins)
            if (static_cast<quint32>(w.pid) == cur)
                return w.address;
    }
    return {};
}

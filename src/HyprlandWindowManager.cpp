#include "HyprlandWindowManager.h"
#include "HyprlandClient.h"

QString HyprlandWindowManager::findWindowByPid(quint32 pid) const
{
    const auto wins = HyprlandClient::clients();
    return HyprlandClient::findWindowAddress(pid, wins);
}

void HyprlandWindowManager::focusWindow(const QString &windowId)
{
    HyprlandClient::focusWindow(windowId);
}

void HyprlandWindowManager::resizeWindow(const QString &windowId, int w, int h)
{
    if (windowId.isEmpty())
        return;
    // Query current position before resize; resizewindowpixel scales from center
    // on floating/layer-shell windows, so we must re-anchor the top-left corner.
    const auto wins = HyprlandClient::clients();
    for (const auto &win : wins) {
        if (win.address == windowId) {
            HyprlandClient::resizeWindow(windowId, w, h, win.x, win.y);
            return;
        }
    }
    HyprlandClient::resizeWindow(windowId, w, h);  // fallback: position unknown
}

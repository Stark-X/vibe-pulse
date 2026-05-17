#include "MacOSWindowManager.h"

#import <AppKit/AppKit.h>

QString MacOSWindowManager::findWindowByPid(quint32 pid) const
{
    // Use PID as the window identifier on macOS
    return pid > 0 ? QString::number(pid) : QString{};
}

void MacOSWindowManager::focusWindow(const QString &windowId)
{
    bool ok = false;
    const pid_t pid = static_cast<pid_t>(windowId.toUInt(&ok));
    if (!ok || pid <= 0)
        return;
    NSRunningApplication *app =
        [NSRunningApplication runningApplicationWithProcessIdentifier: pid];
    if (app)
        [app activateWithOptions: NSApplicationActivateIgnoringOtherApps];
}

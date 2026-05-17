#include "MacOSWindowManager.h"
#include "ProcessTree.h"

#import <AppKit/AppKit.h>

#include <libproc.h>
#include <sys/proc_info.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

QString MacOSWindowManager::findWindowByPid(quint32 pid) const
{
    if (pid <= 0) return {};
    NSRunningApplication *app =
        [NSRunningApplication runningApplicationWithProcessIdentifier: (pid_t)pid];
    // Only return a window ID for actual GUI apps; CLI processes (claude-code, tmux, etc.)
    // have NSApplicationActivationPolicyProhibited and cannot be focused via NSRunningApplication.
    if (!app || app.activationPolicy != NSApplicationActivationPolicyRegular)
        return {};
    return QString::number(pid);
}

QString MacOSWindowManager::findWindowByTTY(const QString &tty) const
{
    if (tty.isEmpty()) return {};

    // Map TTY path → device number
    struct stat st{};
    if (::stat(tty.toLocal8Bit().constData(), &st) != 0)
        return {};
    const dev_t ttyDev = st.st_rdev;

    // Enumerate all processes and collect those whose controlling terminal matches
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    size_t size = 0;
    if (::sysctl(mib, 4, nullptr, &size, nullptr, 0) < 0)
        return {};
    QByteArray buf(static_cast<int>(size) + static_cast<int>(sizeof(kinfo_proc)) * 32, '\0');
    if (::sysctl(mib, 4, buf.data(), &size, nullptr, 0) < 0)
        return {};

    const int count = static_cast<int>(size / sizeof(kinfo_proc));
    const auto *procs = reinterpret_cast<const kinfo_proc *>(buf.constData());

    QVector<quint32> ttyPids;
    for (int i = 0; i < count; ++i) {
        if (procs[i].kp_eproc.e_tdev == ttyDev)
            ttyPids.append(static_cast<quint32>(procs[i].kp_proc.p_pid));
    }

    // Pass 1: ancestor chain from each process on the TTY (works when terminal is
    // a direct parent, e.g. Terminal.app / Alacritty)
    for (quint32 pid : ttyPids) {
        for (quint32 p : ProcessTree::ancestorChain(pid)) {
            const QString addr = findWindowByPid(p);
            if (!addr.isEmpty())
                return addr;
        }
    }

    // Pass 2: PTY master fd check — find a GUI app that holds a char-device fd
    // whose minor number equals the slave's minor (same PTY slot, different major =
    // master side). Works when the terminal spawns shells via launchd/XPC so the
    // shell is NOT a child of the terminal process (e.g. Ghostty).
    {
        const int slaveMinor = minor(ttyDev);
        const int slaveMajor = major(ttyDev);
        int pidCount = proc_listallpids(nullptr, 0);
        if (pidCount > 0) {
            QVector<pid_t> pidBuf(pidCount + 64);
            pidCount = proc_listallpids(pidBuf.data(), static_cast<int>(pidBuf.size() * sizeof(pid_t)));
            for (int i = 0; i < pidCount; ++i) {
                const pid_t pid = pidBuf[i];
                if (pid <= 1) continue;
                NSRunningApplication *app = [NSRunningApplication
                    runningApplicationWithProcessIdentifier:pid];
                if (!app || app.activationPolicy != NSApplicationActivationPolicyRegular)
                    continue;
                const int sz = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, nullptr, 0);
                if (sz <= 0) continue;
                QByteArray fdBuf(sz + static_cast<int>(sizeof(proc_fdinfo)) * 4, '\0');
                const int filled = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, fdBuf.data(), fdBuf.size());
                if (filled <= 0) continue;
                const int fdCnt = filled / static_cast<int>(sizeof(proc_fdinfo));
                const auto *fds = reinterpret_cast<const proc_fdinfo *>(fdBuf.constData());
                for (int j = 0; j < fdCnt; ++j) {
                    if (fds[j].proc_fdtype != PROX_FDTYPE_VNODE) continue;
                    struct vnode_fdinfowithpath vi{};
                    if (proc_pidfdinfo(pid, fds[j].proc_fd, PROC_PIDFDVNODEPATHINFO,
                                       &vi, sizeof(vi)) <= 0) continue;
                    if ((vi.pvip.vip_vi.vi_stat.vst_mode & S_IFMT) != S_IFCHR) continue;
                    const dev_t dev = vi.pvip.vip_vi.vi_stat.vst_rdev;
                    if (minor(dev) == slaveMinor && major(dev) != slaveMajor)
                        return QString::number(static_cast<quint32>(pid));
                }
            }
        }
    }

    // Pass 3: bundle-ID lookup for known terminal emulators — last resort when
    // the PTY master fd is not inspectable (sandboxed app, permissions, etc.).
    {
        static NSArray<NSString *> *terminals = @[
            @"com.mitchellh.ghostty",
            @"com.googlecode.iterm2",
            @"com.apple.Terminal",
            @"dev.warp.Warp-Stable",
            @"io.alacritty",
        ];
        for (NSString *bid in terminals) {
            NSArray<NSRunningApplication *> *apps =
                [NSRunningApplication runningApplicationsWithBundleIdentifier:bid];
            if (apps.count > 0)
                return QString::number(static_cast<quint32>(apps[0].processIdentifier));
        }
    }

    return {};
}

void MacOSWindowManager::focusWindow(const QString &windowId)
{
    bool ok = false;
    const pid_t pid = static_cast<pid_t>(windowId.toUInt(&ok));
    if (!ok || pid <= 0)
        return;
    NSRunningApplication *app =
        [NSRunningApplication runningApplicationWithProcessIdentifier: pid];
    if (!app)
        return;
    // NSWorkspace openApplicationAtURL: activates the already-running instance without
    // requiring Pulse to be the frontmost app (unlike activateFromApplication:options:).
    // Works on macOS 10.15+; falls back to the deprecated API on older systems.
    if (@available(macOS 10.15, *)) {
        if (app.bundleURL) {
            NSWorkspaceOpenConfiguration *config = [NSWorkspaceOpenConfiguration configuration];
            config.activates = YES;
            [[NSWorkspace sharedWorkspace] openApplicationAtURL:app.bundleURL
                                                  configuration:config
                                              completionHandler:nil];
            return;
        }
    }
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
#pragma clang diagnostic pop
}

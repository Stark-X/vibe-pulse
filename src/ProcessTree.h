#pragma once
#include <QtGlobal>
#include <QString>
#include <QStringList>
#include <QVector>

// Platform-agnostic process information access.
// Implementations: ProcessTree_linux.cpp / _macos.cpp / _win.cpp
class ProcessTree {
public:
    static QString     comm(quint32 pid);
    static QString     exe(quint32 pid);
    static QString     cwd(quint32 pid);       // best-effort; may return empty
    static QStringList cmdline(quint32 pid);
    static quint32     ppid(quint32 pid);      // 0 if unknown

    // Walk parent chain up to maxDepth hops; cycle-safe.
    // Result includes startPid as first element.
    static QVector<quint32> ancestorChain(quint32 startPid, int maxDepth = 32);

    // All active main-thread PIDs on the system.
    static QVector<quint32> listAll();

    // Returns the path of the first open file on this process whose path
    // contains `needle` and ends with `suffix`.
    // Linux: /proc/<pid>/fd symlinks. macOS: PROC_PIDLISTFDS.
    // Windows: always returns empty (no portable per-process fd enumeration).
    static QString openFileMatching(quint32 pid,
                                    const QString &needle,
                                    const QString &suffix);
};

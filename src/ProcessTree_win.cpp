#include "ProcessTree.h"

#include <QFileInfo>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>

QVector<quint32> ProcessTree::listAll()
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return {};
    QVector<quint32> result;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (pe.th32ProcessID > 0)
            result.append(static_cast<quint32>(pe.th32ProcessID));
    CloseHandle(snap);
    return result;
}

quint32 ProcessTree::ppid(quint32 pid)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return 0;
    quint32 result = 0;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (pe.th32ProcessID == pid) {
            result = static_cast<quint32>(pe.th32ParentProcessID);
            break;
        }
    }
    CloseHandle(snap);
    return result;
}

QString ProcessTree::exe(quint32 pid)
{
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                           static_cast<DWORD>(pid));
    if (!h)
        return {};
    wchar_t buf[MAX_PATH] = {};
    DWORD sz = MAX_PATH;
    const bool ok = QueryFullProcessImageNameW(h, 0, buf, &sz);
    CloseHandle(h);
    return ok ? QString::fromWCharArray(buf, static_cast<int>(sz)) : QString{};
}

QString ProcessTree::comm(quint32 pid)
{
    const QString path = exe(pid);
    if (path.isEmpty())
        return {};
    return QFileInfo(path).completeBaseName();
}

// CWD is not reliably accessible for arbitrary processes on Windows.
QString ProcessTree::cwd(quint32) { return {}; }

// Cmdline via NtQueryInformationProcess is complex and uses internal APIs.
// Return empty for now; agent detection uses comm/exe matching only.
QStringList ProcessTree::cmdline(quint32) { return {}; }

QVector<quint32> ProcessTree::ancestorChain(quint32 startPid, int maxDepth)
{
    QVector<quint32> chain;
    chain.reserve(maxDepth + 1);
    chain.append(startPid);
    for (int i = 0; i < maxDepth; ++i) {
        const quint32 parent = ppid(chain.last());
        if (parent == 0 || chain.contains(parent))
            break;
        chain.append(parent);
    }
    return chain;
}

#include "ProcessTree.h"

#include <QFileInfo>

#include <libproc.h>
#include <sys/sysctl.h>

QVector<quint32> ProcessTree::listAll()
{
    const int n = proc_listallpids(nullptr, 0);
    if (n <= 0)
        return {};
    QVector<pid_t> buf(n + 32);
    const int count = proc_listallpids(buf.data(), buf.size() * sizeof(pid_t));
    QVector<quint32> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i)
        if (buf[i] > 0)
            result.append(static_cast<quint32>(buf[i]));
    return result;
}

QString ProcessTree::comm(quint32 pid)
{
    struct proc_bsdinfo info{};
    if (proc_pidinfo(static_cast<int>(pid), PROC_PIDTBSDINFO, 0,
                     &info, sizeof(info)) <= 0)
        return {};
    return QString::fromLocal8Bit(info.pbi_comm);
}

QString ProcessTree::exe(quint32 pid)
{
    char buf[PROC_PIDPATHINFO_MAXSIZE] = {};
    if (proc_pidpath(static_cast<int>(pid), buf, sizeof(buf)) <= 0)
        return {};
    return QString::fromLocal8Bit(buf);
}

QString ProcessTree::cwd(quint32 pid)
{
    struct proc_vnodepathinfo vpi{};
    if (proc_pidinfo(static_cast<int>(pid), PROC_PIDVNODEPATHINFO, 0,
                     &vpi, sizeof(vpi)) <= 0)
        return {};
    return QString::fromLocal8Bit(vpi.pvi_cdir.vip_path);
}

QStringList ProcessTree::cmdline(quint32 pid)
{
    int mib[3] = { CTL_KERN, KERN_PROCARGS2, static_cast<int>(pid) };
    size_t size = 0;
    if (sysctl(mib, 3, nullptr, &size, nullptr, 0) < 0 || size == 0)
        return {};
    QByteArray buf(static_cast<int>(size), '\0');
    if (sysctl(mib, 3, buf.data(), &size, nullptr, 0) < 0)
        return {};
    // KERN_PROCARGS2 layout: [argc int32][exe\0][arg0\0][arg1\0]...
    if (static_cast<int>(size) < 4)
        return {};
    int argc = 0;
    memcpy(&argc, buf.data(), sizeof(argc));
    // Skip argc + exe path
    int pos = 4;
    while (pos < static_cast<int>(size) && buf[pos] != '\0') ++pos; // skip exe
    while (pos < static_cast<int>(size) && buf[pos] == '\0') ++pos; // skip nuls
    QStringList args;
    for (int i = 0; i < argc && pos < static_cast<int>(size); ++i) {
        const char *start = buf.constData() + pos;
        const int len = static_cast<int>(strnlen(start, size - pos));
        args << QString::fromLocal8Bit(start, len);
        pos += len + 1;
    }
    return args;
}

quint32 ProcessTree::ppid(quint32 pid)
{
    struct proc_bsdinfo info{};
    if (proc_pidinfo(static_cast<int>(pid), PROC_PIDTBSDINFO, 0,
                     &info, sizeof(info)) <= 0)
        return 0;
    return static_cast<quint32>(info.pbi_ppid);
}

QVector<quint32> ProcessTree::ancestorChain(quint32 startPid, int maxDepth)
{
    QVector<quint32> chain;
    chain.reserve(maxDepth + 1);
    chain.append(startPid);
    for (int i = 0; i < maxDepth; ++i) {
        const quint32 parent = ppid(chain.last());
        if (parent == 0 || parent == 1 || chain.contains(parent))
            break;
        chain.append(parent);
    }
    return chain;
}

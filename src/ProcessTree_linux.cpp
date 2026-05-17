#include "ProcessTree.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

QVector<quint32> ProcessTree::listAll()
{
    const QDir procDir(QStringLiteral("/proc"));
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QVector<quint32> result;
    result.reserve(entries.size());
    for (const QString &e : entries) {
        bool ok = false;
        const quint32 pid = e.toUInt(&ok);
        if (!ok)
            continue;
        // Only main threads: Tgid == Pid
        QFile f(QStringLiteral("/proc/%1/status").arg(pid));
        if (!f.open(QIODevice::ReadOnly))
            continue;
        quint32 tgid = 0, pidVal = 0;
        for (const QByteArray &line : f.readAll().split('\n')) {
            if (line.startsWith("Pid:"))
                pidVal = line.mid(4).trimmed().toUInt();
            else if (line.startsWith("Tgid:"))
                tgid = line.mid(5).trimmed().toUInt();
        }
        if (pidVal != 0 && pidVal == tgid)
            result.append(pid);
    }
    return result;
}

QString ProcessTree::comm(quint32 pid)
{
    QFile f(QStringLiteral("/proc/%1/comm").arg(pid));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromLocal8Bit(f.readAll()).trimmed();
}

QString ProcessTree::exe(quint32 pid)
{
    return QFileInfo(QStringLiteral("/proc/%1/exe").arg(pid)).symLinkTarget();
}

QString ProcessTree::cwd(quint32 pid)
{
    return QFileInfo(QStringLiteral("/proc/%1/cwd").arg(pid)).symLinkTarget();
}

QStringList ProcessTree::cmdline(quint32 pid)
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

quint32 ProcessTree::ppid(quint32 pid)
{
    QFile f(QStringLiteral("/proc/%1/status").arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    for (const QByteArray &line : f.readAll().split('\n'))
        if (line.startsWith("PPid:"))
            return line.mid(5).trimmed().toUInt();
    return 0;
}

// ancestorChain is implemented in ProcessTree.cpp (shared)

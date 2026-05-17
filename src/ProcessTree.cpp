#include "ProcessTree.h"

// ancestorChain is platform-independent (delegates to platform ppid()).
// Defined here once to avoid duplication in platform files.
QVector<quint32> ProcessTree::ancestorChain(quint32 startPid, int maxDepth)
{
    QVector<quint32> chain;
    chain.reserve(maxDepth);
    chain.append(startPid);
    for (int i = 1; i < maxDepth; ++i) {
        const quint32 parent = ppid(chain.last());
        if (parent == 0 || parent == 1 || chain.contains(parent))
            break;
        chain.append(parent);
    }
    return chain;
}

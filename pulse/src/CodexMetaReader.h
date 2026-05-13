#pragma once
#include <QString>

struct CodexMeta {
    QString sessionId;
    QString sessionName;
    QString currentStep;
};

class CodexMetaReader
{
public:
    static CodexMeta read(quint32 pid, QStringList *watchPaths = nullptr);
};

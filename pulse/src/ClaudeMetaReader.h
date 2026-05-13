#pragma once
#include <QString>

struct ClaudeMeta {
    QString sessionId;
    QString sessionName;
    bool    sessionBusy = true;
    QString currentStep;
    QString cwd;
};

class ClaudeMetaReader
{
public:
    static ClaudeMeta read(quint32 pid, const QString &cwd,
                            QStringList *watchPaths = nullptr);
};

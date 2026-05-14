#pragma once
#include <QString>

class ResponseWriter
{
public:
    static bool writeQuestionAnswer(const QString &sessionId,
                                    const QString &interactionId,
                                    int optionIndex,
                                    const QString &answer);
    static bool writePlanDecision(const QString &sessionId,
                                  const QString &interactionId,
                                  const QString &decision,
                                  const QString &comment = {});
    static bool writePermissionDecision(const QString &sessionId,
                                        const QString &interactionId,
                                        bool allow,
                                        const QString &amendment = {});
};

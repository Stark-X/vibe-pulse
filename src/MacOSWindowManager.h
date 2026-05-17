#pragma once
#include "WindowManager.h"

class MacOSWindowManager final : public WindowManager {
public:
    QString findWindowByPid(quint32 pid) const override;
    QString findWindowByTTY(const QString &tty) const override;
    void    focusWindow(const QString &windowId) override;
    void    resizeWindow(const QString &, int, int) override {}
    bool    needsExternalResize() const override { return false; }
};

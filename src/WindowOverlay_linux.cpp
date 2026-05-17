#include "WindowOverlay.h"
#include "WaylandLayerShell.h"

#include <QWindow>

class LinuxWindowOverlay final : public WindowOverlay {
    WaylandLayerShell *m_shell = nullptr;
public:
    void setup(QWindow *win) override {
        m_shell = new WaylandLayerShell(win);
        if (!m_shell->isValid())
            qWarning("pulse: layer-shell unavailable, running as normal window");
    }
    ~LinuxWindowOverlay() override { delete m_shell; }
};

std::unique_ptr<WindowOverlay> WindowOverlay::create()
{
    return std::make_unique<LinuxWindowOverlay>();
}

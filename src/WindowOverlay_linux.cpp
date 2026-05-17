#include "WindowOverlay.h"
#include "WaylandLayerShell.h"

#include <QWindow>

#include <memory>

class LinuxWindowOverlay final : public WindowOverlay {
    std::unique_ptr<WaylandLayerShell> m_shell;
public:
    void setup(QWindow *win, const OverlayOptions &) override {
        m_shell = std::make_unique<WaylandLayerShell>(win);
        if (!m_shell->isValid())
            qWarning("pulse: layer-shell unavailable, running as normal window");
    }
};

std::unique_ptr<WindowOverlay> WindowOverlay::create()
{
    return std::make_unique<LinuxWindowOverlay>();
}

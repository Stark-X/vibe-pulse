#pragma once
#include <QString>
#include <memory>

// Abstract interface for compositor-specific window operations.
// Provides "jump to terminal" (find + focus by PID) and own-window resize.
// Use WindowManager::create() to obtain the correct implementation at runtime.
class WindowManager {
public:
    virtual ~WindowManager() = default;

    // Find the compositor window ID for the process with the given PID.
    // Walks the parent process chain. Returns empty string if not found or unsupported.
    virtual QString findWindowByPid(quint32 pid) const = 0;

    // Focus the window identified by a compositor-specific ID.
    virtual void focusWindow(const QString &windowId) = 0;

    // Resize the pulse overlay window identified by windowId to (w, h) pixels.
    // Implementations that need position correction (e.g. Hyprland IPC resizes
    // from center) should query and restore position internally.
    virtual void resizeWindow(const QString &windowId, int w, int h) = 0;

    // True if this compositor requires external IPC to resize the pulse window.
    // False means wlr-layer-shell set_size handles it — no extra step needed.
    virtual bool needsExternalResize() const = 0;

    // Detect the running compositor and return the appropriate implementation.
    static std::unique_ptr<WindowManager> create();
};

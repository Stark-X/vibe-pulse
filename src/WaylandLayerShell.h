#pragma once

#include <QWindow>

#include <cstdint>

struct wl_display;
struct wl_registry;
struct wl_surface;
struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

class WaylandLayerShell
{
public:
    explicit WaylandLayerShell(QWindow *window);
    ~WaylandLayerShell();

    WaylandLayerShell(const WaylandLayerShell &) = delete;
    WaylandLayerShell &operator=(const WaylandLayerShell &) = delete;
    WaylandLayerShell(WaylandLayerShell &&) = delete;
    WaylandLayerShell &operator=(WaylandLayerShell &&) = delete;

    bool isValid() const noexcept { return m_valid; }

    static void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
    static void registryGlobalRemove(void *data, wl_registry *registry, uint32_t name);
    static void layerSurfaceConfigure(void *data, zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height);
    static void layerSurfaceClosed(void *data, zwlr_layer_surface_v1 *surface);

private:

    void initialize();
    void cleanup();

    QWindow *m_window = nullptr;
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_surface *m_surface = nullptr;
    zwlr_layer_shell_v1 *m_layerShell = nullptr;
    zwlr_layer_surface_v1 *m_layerSurface = nullptr;
    uint32_t m_layerShellVersion = 0;
    bool m_valid = false;
};

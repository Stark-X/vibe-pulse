#include "LayerShell.h"

#include <QGuiApplication>
#include <QtGui/6.4.2/QtGui/qpa/qplatformnativeinterface.h>

#include <wayland-client.h>

// ── LayerShell ────────────────────────────────────────────────────────────────

LayerShell::LayerShell()
    : QWaylandClientExtensionTemplate<LayerShell>(4)
{
    initialize();
}

LayerShell::~LayerShell()
{
    if (isActive())
        destroy();
}

LayerSurface *LayerShell::createSurface(QWindow *window)
{
    if (!isActive())
        return nullptr;

    auto *ni = QGuiApplication::platformNativeInterface();
    if (!ni)
        return nullptr;

    auto *wlSurface = static_cast<struct wl_surface *>(
        ni->nativeResourceForWindow(QByteArrayLiteral("surface"), window));
    if (!wlSurface)
        return nullptr;

    struct ::zwlr_layer_surface_v1 *layerSurf =
        get_layer_surface(wlSurface, nullptr,
                          QtWayland::zwlr_layer_shell_v1::layer_overlay,
                          QStringLiteral("pulse"));

    auto *surface = new LayerSurface(layerSurf, window, this);
    return surface;
}

// ── LayerSurface ──────────────────────────────────────────────────────────────

LayerSurface::LayerSurface(struct ::zwlr_layer_surface_v1 *surface,
                           QWindow *window,
                           QObject *parent)
    : QObject(parent)
    , QtWayland::zwlr_layer_surface_v1(surface)
    , m_window(window)
{
    // anchor = TOP | RIGHT
    set_anchor(QtWayland::zwlr_layer_surface_v1::anchor_top |
               QtWayland::zwlr_layer_surface_v1::anchor_right);
    set_exclusive_zone(-1);
    set_keyboard_interactivity(
        QtWayland::zwlr_layer_surface_v1::keyboard_interactivity_none);
    // size 0,0 → compositor assigns; we'll set explicit size after configure
    set_size(0, 0);
    set_margin(8, 8, 0, 0);
}

LayerSurface::~LayerSurface()
{
    destroy();
}

void LayerSurface::zwlr_layer_surface_v1_configure(uint32_t serial,
                                                    uint32_t width,
                                                    uint32_t height)
{
    ack_configure(serial);
    if (width > 0 && height > 0)
        m_window->resize(static_cast<int>(width), static_cast<int>(height));
    emit configured();
}

void LayerSurface::zwlr_layer_surface_v1_closed()
{
    emit closed();
}

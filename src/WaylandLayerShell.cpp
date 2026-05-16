#include "WaylandLayerShell.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

#include <algorithm>
#include <cstring>

#include <wayland-client.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

namespace {
constexpr uint32_t kDefaultWidth = 320;
constexpr uint32_t kDefaultHeight = 280;
constexpr const char kLayerNamespace[] = "pulse";

static const wl_registry_listener kRegistryListener = {
    WaylandLayerShell::registryGlobal,
    WaylandLayerShell::registryGlobalRemove,
};

static const zwlr_layer_surface_v1_listener kLayerSurfaceListener = {
    WaylandLayerShell::layerSurfaceConfigure,
    WaylandLayerShell::layerSurfaceClosed,
};
} // namespace

WaylandLayerShell::WaylandLayerShell(QWindow *window)
    : m_window(window)
{
    initialize();
}

WaylandLayerShell::~WaylandLayerShell()
{
    cleanup();
}

void WaylandLayerShell::initialize()
{
    if (!m_window) {
        qWarning() << "pulse: missing QWindow";
        return;
    }

    m_window->create();

    auto *nativeInterface = QGuiApplication::platformNativeInterface();
    if (!nativeInterface) {
        qWarning() << "pulse: no QPlatformNativeInterface";
        return;
    }

    m_display = static_cast<wl_display *>(nativeInterface->nativeResourceForIntegration(QByteArrayLiteral("display")));
    m_surface = static_cast<wl_surface *>(nativeInterface->nativeResourceForWindow(QByteArrayLiteral("surface"), m_window));
    if (!m_display || !m_surface) {
        qWarning() << "pulse: unable to resolve Wayland display or surface";
        return;
    }

    m_registry = wl_display_get_registry(m_display);
    if (!m_registry) {
        qWarning() << "pulse: unable to obtain wl_registry";
        return;
    }

    wl_registry_add_listener(m_registry, &kRegistryListener, this);

    if (wl_display_roundtrip(m_display) < 0) {
        qWarning() << "pulse: registry roundtrip failed";
        return;
    }

    if (!m_layerShell) {
        qWarning() << "pulse: zwlr_layer_shell_v1 not advertised";
        return;
    }

    m_layerSurface = zwlr_layer_shell_v1_get_layer_surface(
        m_layerShell,
        m_surface,
        nullptr,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        kLayerNamespace);
    if (!m_layerSurface) {
        qWarning() << "pulse: unable to create zwlr_layer_surface_v1";
        return;
    }

    zwlr_layer_surface_v1_add_listener(m_layerSurface, &kLayerSurfaceListener, this);
    zwlr_layer_surface_v1_set_anchor(
        m_layerSurface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(m_layerSurface, -1);
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        m_layerSurface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);
    zwlr_layer_surface_v1_set_size(
        m_layerSurface,
        static_cast<uint32_t>(m_window->width() > 0 ? m_window->width() : kDefaultWidth),
        static_cast<uint32_t>(m_window->height() > 0 ? m_window->height() : kDefaultHeight));

    if (m_layerShellVersion >= 2) {
        zwlr_layer_surface_v1_set_layer(m_layerSurface, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY);
    }

    wl_surface_commit(m_surface);

    if (wl_display_roundtrip(m_display) < 0) {
        qWarning() << "pulse: configure roundtrip failed";
        return;
    }

    m_valid = true;
}

void WaylandLayerShell::requestResize(int width, int height)
{
    if (!m_valid || !m_window)
        return;
    m_pendingW = width;
    m_pendingH = height;
    m_resizePending = true;
    // Only resize via QWindow — do NOT call zwlr_layer_surface_v1_set_size
    // or wl_surface_commit here. Qt's Wayland backend owns the display
    // connection; calling libwayland-client APIs concurrently crashes.
    // Instead, store the pending size and apply it during the next
    // configure callback from the compositor.
    m_window->resize(width, height);
}

void WaylandLayerShell::setSize(int width, int height)
{
    if (!m_valid || !m_layerSurface || !m_window)
        return;
    // Call set_size directly from the main thread — safe because Qt's Wayland
    // event dispatch also runs on the main thread (via QSocketNotifier).
    // The request is buffered by libwayland and submitted on Qt's next
    // wl_surface_commit(), which respects anchor_top|anchor_right so the
    // window grows downward instead of from the center.
    zwlr_layer_surface_v1_set_size(m_layerSurface,
                                    static_cast<uint32_t>(width),
                                    static_cast<uint32_t>(height));
    m_window->resize(width, height);
}

void WaylandLayerShell::applyPendingResize()
{
    if (!m_resizePending || !m_layerSurface)
        return;
    m_resizePending = false;
    zwlr_layer_surface_v1_set_size(m_layerSurface,
                                    static_cast<uint32_t>(m_pendingW),
                                    static_cast<uint32_t>(m_pendingH));
}

void WaylandLayerShell::cleanup()
{
    if (m_layerSurface) {
        zwlr_layer_surface_v1_destroy(m_layerSurface);
        m_layerSurface = nullptr;
    }

    if (m_layerShell) {
        if (m_layerShellVersion >= 3) {
            zwlr_layer_shell_v1_destroy(m_layerShell);
        }
        m_layerShell = nullptr;
    }

    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }

    m_surface = nullptr;
    m_display = nullptr;
    m_layerShellVersion = 0;
    m_valid = false;
}

void WaylandLayerShell::registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    auto *self = static_cast<WaylandLayerShell *>(data);
    if (!self || !interface) {
        return;
    }

    if (std::strcmp(interface, zwlr_layer_shell_v1_interface.name) != 0) {
        return;
    }

    self->m_layerShellVersion = std::min<uint32_t>(version, zwlr_layer_shell_v1_interface.version);
    self->m_layerShell = static_cast<zwlr_layer_shell_v1 *>(
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, self->m_layerShellVersion));
}

void WaylandLayerShell::registryGlobalRemove(void *data, wl_registry *registry, uint32_t name)
{
    Q_UNUSED(data);
    Q_UNUSED(registry);
    Q_UNUSED(name);
}

void WaylandLayerShell::layerSurfaceConfigure(void *data, zwlr_layer_surface_v1 *surface, uint32_t serial, uint32_t width, uint32_t height)
{
    auto *self = static_cast<WaylandLayerShell *>(data);
    if (!self || !surface)
        return;

    Q_UNUSED(width);
    Q_UNUSED(height);

    // Apply pending resize before ack so compositor uses our requested size.
    self->applyPendingResize();
    zwlr_layer_surface_v1_ack_configure(surface, serial);
}

void WaylandLayerShell::layerSurfaceClosed(void *data, zwlr_layer_surface_v1 *surface)
{
    auto *self = static_cast<WaylandLayerShell *>(data);
    Q_UNUSED(surface);
    if (!self) {
        return;
    }

    QCoreApplication::quit();
}

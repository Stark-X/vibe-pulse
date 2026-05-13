#pragma once

#include <QObject>
#include <QWindow>
#include <QtWaylandClient/QWaylandClientExtension>

#include "qwayland-wlr-layer-shell-unstable-v1.h"

class LayerSurface;

class LayerShell : public QWaylandClientExtensionTemplate<LayerShell>,
                   public QtWayland::zwlr_layer_shell_v1
{
    Q_OBJECT
public:
    explicit LayerShell();
    ~LayerShell() override;

    LayerSurface *createSurface(QWindow *window);

private:
    Q_DISABLE_COPY(LayerShell)
};

class LayerSurface : public QObject,
                     public QtWayland::zwlr_layer_surface_v1
{
    Q_OBJECT
public:
    explicit LayerSurface(struct ::zwlr_layer_surface_v1 *surface, QWindow *window, QObject *parent = nullptr);
    ~LayerSurface() override;

    void configure(uint32_t width, uint32_t height);

signals:
    void configured();
    void closed();

protected:
    void zwlr_layer_surface_v1_configure(uint32_t serial, uint32_t width, uint32_t height) override;
    void zwlr_layer_surface_v1_closed() override;

private:
    QWindow *m_window;
};

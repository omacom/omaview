#include "inhibit.h"

#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <cstring>
#include <wayland-client.h>

// The one private header in Omaview: Qt hands out a window's wl_surface only
// through QPlatformNativeInterface. Calling it is a virtual call, not a link
// against Qt's private symbols, so the package is not tied to one Qt version.
#include <qpa/qplatformnativeinterface.h>

#include "idle-inhibit-unstable-v1-client-protocol.h"

namespace {

void registryGlobalRemove(void *, wl_registry *, uint32_t) {}

const wl_registry_listener registryListener = {&IdleInhibitor::registryGlobal, &registryGlobalRemove};

void flushWayland() {
    if (auto *wayland = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
        wayland && wayland->display())
        wl_display_flush(wayland->display());
}

}  // namespace

void IdleInhibitor::registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface,
                                   uint32_t) {
    auto *self = static_cast<IdleInhibitor *>(data);
    if (std::strcmp(interface, zwp_idle_inhibit_manager_v1_interface.name) == 0 && !self->m_manager) {
        self->m_manager = static_cast<zwp_idle_inhibit_manager_v1 *>(
            wl_registry_bind(registry, name, &zwp_idle_inhibit_manager_v1_interface, 1));
    }
}

IdleInhibitor::IdleInhibitor(QObject *parent) : QObject(parent) {
    auto *wayland = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
    if (!wayland || !wayland->display())
        return;
    // A queue of Omaview's own, so asking for the globals never dispatches
    // events Qt's Wayland thread is waiting for.
    wl_display *display = wayland->display();
    wl_event_queue *queue = wl_display_create_queue(display);
    if (!queue)
        return;
    auto *wrapper = static_cast<wl_display *>(wl_proxy_create_wrapper(display));
    if (!wrapper) {
        wl_event_queue_destroy(queue);
        return;
    }
    wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(wrapper), queue);
    wl_registry *registry = wl_display_get_registry(wrapper);
    wl_proxy_wrapper_destroy(wrapper);
    if (registry) {
        wl_registry_add_listener(registry, &registryListener, this);
        wl_display_roundtrip_queue(display, queue);
        // Neither the manager nor its inhibitors have events; detach from the
        // temporary queue so it and the registry can be freed right away rather
        // than accumulating future global events undispatched.
        if (m_manager)
            wl_proxy_set_queue(reinterpret_cast<wl_proxy *>(m_manager), nullptr);
        wl_registry_destroy(registry);
    }
    wl_event_queue_destroy(queue);
}

IdleInhibitor::~IdleInhibitor() {
    if (m_window)
        m_window->removeEventFilter(this);
    release();
    if (m_manager) {
        zwp_idle_inhibit_manager_v1_destroy(m_manager);
        flushWayland();
    }
}

void IdleInhibitor::setActive(bool active) {
    if (m_active == active)
        return;
    m_active = active;
    update();
    emit activeChanged();
}

void IdleInhibitor::setWindow(QWindow *window) {
    if (m_window == window)
        return;
    if (m_window) {
        m_window->removeEventFilter(this);
        disconnect(m_window, nullptr, this, nullptr);
    }
    release();
    m_window = window;
    // The surface comes and goes with the window's visibility and platform
    // surface lifecycle, so the inhibitor follows both.
    if (m_window) {
        m_window->installEventFilter(this);
        connect(m_window, &QWindow::visibleChanged, this, [this] {
            release();
            update();
        });
    }
    update();
    emit windowChanged();
}

bool IdleInhibitor::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_window && event->type() == QEvent::PlatformSurface) {
        const auto type = static_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType();
        if (type == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            release();
        else if (type == QPlatformSurfaceEvent::SurfaceCreated)
            update();
    }
    return QObject::eventFilter(watched, event);
}

void IdleInhibitor::release() {
    if (m_inhibitor) {
        zwp_idle_inhibitor_v1_destroy(m_inhibitor);
        m_inhibitor = nullptr;
        flushWayland();
    }
    m_surface = nullptr;
}

void IdleInhibitor::update() {
    if (!m_active || !m_manager || !m_window || !m_window->isVisible()) {
        release();
        return;
    }
    auto *native = QGuiApplication::platformNativeInterface();
    void *surface = native ? native->nativeResourceForWindow("surface", m_window) : nullptr;
    if (surface == m_surface && m_inhibitor)
        return;
    release();
    if (!surface)
        return;
    m_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(m_manager, static_cast<wl_surface *>(surface));
    m_surface = surface;
    flushWayland();
}

#pragma once

#include <QObject>
#include <QPointer>
#include <QWindow>

struct wl_registry;
struct zwp_idle_inhibit_manager_v1;
struct zwp_idle_inhibitor_v1;

// Keeps the screen on while a video plays, with a Wayland idle inhibitor on
// the window's own surface: Hyprland applies it, and Omarchy's idle monitor
// respects inhibitors. Does nothing off Wayland or without the protocol.
class IdleInhibitor : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)

public:
    explicit IdleInhibitor(QObject *parent = nullptr);
    ~IdleInhibitor() override;

    bool active() const { return m_active; }
    void setActive(bool active);
    QWindow *window() const { return m_window; }
    void setWindow(QWindow *window);

    static void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface,
                               uint32_t version);

signals:
    void activeChanged();
    void windowChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void update();
    void release();

    bool m_active = false;
    QPointer<QWindow> m_window;
    zwp_idle_inhibit_manager_v1 *m_manager = nullptr;
    zwp_idle_inhibitor_v1 *m_inhibitor = nullptr;
    void *m_surface = nullptr;
};

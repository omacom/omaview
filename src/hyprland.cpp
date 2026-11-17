#include "hyprland.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QString>

#include <utility>

namespace hyprland {

bool reshape(QSize size) {
    const QByteArray instance = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");
    const QByteArray runtime = qgetenv("XDG_RUNTIME_DIR");
    if (QGuiApplication::platformName() != QLatin1String("wayland") || instance.isEmpty() || runtime.isEmpty()
        || size.isEmpty())
        return false;
    QLocalSocket socket;
    socket.connectToServer(QString::fromLocal8Bit(runtime + "/hypr/" + instance + "/.socket.sock"));
    if (!socket.waitForConnected(200))
        return false;
    const QString window = QStringLiteral("pid:%1").arg(QCoreApplication::applicationPid());
    const QString batch =
        QStringLiteral("[[BATCH]]dispatch hl.dsp.window.resize({ window = \"%1\", x = %2, y = %3 });"
                       "dispatch hl.dsp.window.center({ window = \"%1\" })")
            .arg(window)
            .arg(size.width())
            .arg(size.height());
    socket.write(batch.toUtf8());
    if (!socket.waitForBytesWritten(200) || !socket.waitForReadyRead(200))
        return false;
    return socket.readAll().startsWith("ok");
}

QSize focusedArea() {
    const QByteArray instance = qgetenv("HYPRLAND_INSTANCE_SIGNATURE");
    const QByteArray runtime = qgetenv("XDG_RUNTIME_DIR");
    if (QGuiApplication::platformName() != QLatin1String("wayland") || instance.isEmpty() || runtime.isEmpty())
        return {};
    QLocalSocket socket;
    socket.connectToServer(QString::fromLocal8Bit(runtime + "/hypr/" + instance + "/.socket.sock"));
    if (!socket.waitForConnected(200))
        return {};
    socket.write("j/monitors");
    if (!socket.waitForBytesWritten(200))
        return {};
    QByteArray reply;
    while (socket.waitForReadyRead(200))
        reply += socket.readAll();
    const QJsonArray monitors = QJsonDocument::fromJson(reply).array();
    for (const QJsonValue &value : monitors) {
        const QJsonObject monitor = value.toObject();
        if (!monitor.value("focused").toBool())
            continue;
        const double scale = monitor.value("scale").toDouble(1) > 0 ? monitor.value("scale").toDouble(1) : 1;
        double width = monitor.value("width").toDouble() / scale, height = monitor.value("height").toDouble() / scale;
        if (monitor.value("transform").toInt() % 2 == 1)
            std::swap(width, height);
        const QJsonArray reserved = monitor.value("reserved").toArray();
        if (reserved.size() == 4) {
            width -= reserved.at(0).toDouble() + reserved.at(2).toDouble();
            height -= reserved.at(1).toDouble() + reserved.at(3).toDouble();
        }
        return width > 0 && height > 0 ? QSize(qRound(width), qRound(height)) : QSize();
    }
    return {};
}

}  // namespace hyprland

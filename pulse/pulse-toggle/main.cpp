#include <unistd.h>
#include <QCoreApplication>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString socketPath =
        QStringLiteral("/tmp/pulse-%1.sock").arg(getuid());

    QLocalSocket socket;
    socket.connectToServer(socketPath);
    if (!socket.waitForConnected(1000)) {
        qWarning("pulse not running or socket not found: %s",
                 qPrintable(socketPath));
        return 1;
    }

    QJsonObject msg;
    msg["cmd"] = "toggle";
    socket.write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n');
    socket.waitForBytesWritten(500);
    return 0;
}

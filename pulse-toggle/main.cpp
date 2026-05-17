#include <QCoreApplication>
#include <QLocalSocket>
#include <QJsonDocument>
#include <QJsonObject>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString cmd = (argc > 1) ? QString::fromLocal8Bit(argv[1])
                                   : QStringLiteral("toggle");

    QLocalSocket socket;
    socket.connectToServer(QStringLiteral("pulse-ipc"));
    if (!socket.waitForConnected(1000)) {
        qWarning("pulse not running");
        return 1;
    }

    QJsonObject msg;
    msg[QStringLiteral("cmd")] = cmd;
    socket.write(QJsonDocument(msg).toJson(QJsonDocument::Compact) + '\n');
    socket.waitForBytesWritten(500);
    return 0;
}

#include "networking/Message.h"
#include "networking/TcpPeer.h"
#include "networking/UdpDiscovery.h"

#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    const QString text = QString::fromUtf8("Hello Köln 👋\nsecond line");
    const auto encoded = Message::encode("text", "Alice", {{"text", text}});
    if (!Message::decode(encoded) || Message::decode("not JSON") || Message::decode("{}") ||
        Message::decode(QByteArray(Message::MaxBytes + 1, 'x')) || Message::validText("  ") ||
        Message::validText(QString(Message::MaxTextLength + 1, 'x'))) {
        qCritical() << "Message validation failed";
        return 1;
    }

    TcpPeer alice("Alice"), bob("Bob");
    UdpDiscovery udpAlice("Alice"), udpBob("Bob");
    bool udpForward = false, udpBackward = false, tcpForward = false, tcpBackward = false;
    bool peerSeen = false;
    auto finish = [&] {
        if (udpForward && udpBackward && tcpForward && tcpBackward && peerSeen) {
            qInfo() << "PASS: UDP discovery and bidirectional UDP/TCP Unicode text";
            app.exit(0);
        }
    };
    auto fail = [&](const QString& error) {
        qCritical().noquote() << error;
        app.exit(1);
    };
    QObject::connect(&udpAlice, &UdpDiscovery::errorOccurred, &app, fail);
    QObject::connect(&udpBob, &UdpDiscovery::errorOccurred, &app, fail);
    QObject::connect(&alice, &TcpPeer::errorOccurred, &app, fail);
    QObject::connect(&bob, &TcpPeer::errorOccurred, &app, fail);
    QObject::connect(&udpBob, &UdpDiscovery::peerDiscovered, &app,
                     [&](const QString& sender, const QString&, int port) {
                         if (sender == "Alice" && port == alice.port()) {
                             peerSeen = true;
                             finish();
                         }
                     });
    QObject::connect(&udpBob, &UdpDiscovery::messageReceived, &app,
                     [&](const QString& sender, const QString& received) {
                         udpForward = sender == "Alice" && received == text;
                         udpBob.sendText(text);
                         finish();
                     });
    QObject::connect(&udpAlice, &UdpDiscovery::messageReceived, &app,
                     [&](const QString& sender, const QString& received) {
                         udpBackward = sender == "Bob" && received == text;
                         finish();
                     });
    QObject::connect(&alice, &TcpPeer::connectionChanged, &app, [&] {
        if (alice.isConnected())
            alice.sendText(text);
    });
    QObject::connect(&bob, &TcpPeer::messageReceived, &app,
                     [&](const QString& sender, const QString& received) {
                         tcpForward = sender == "Alice" && received == text;
                         bob.sendText(text);
                         finish();
                     });
    QObject::connect(&alice, &TcpPeer::messageReceived, &app,
                     [&](const QString& sender, const QString& received) {
                         tcpBackward = sender == "Bob" && received == text;
                         finish();
                     });
    QTimer::singleShot(10000, &app, [&] { fail("Timed out waiting for UDP/TCP exchange"); });
    QTimer::singleShot(0, &app, [&] {
        if (!alice.listen() || !bob.listen() ||
            !udpAlice.start(static_cast<quint16>(alice.port())) ||
            !udpBob.start(static_cast<quint16>(bob.port()))) {
            app.exit(1);
            return;
        }
        alice.connectToPeer("127.0.0.1", bob.port());
        udpAlice.sendText(text);
    });
    return app.exec();
}

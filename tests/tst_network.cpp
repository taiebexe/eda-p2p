// Integrationstest: echte Sockets über localhost (127.0.0.1).
// Prüft, dass UdpService und TcpService zusammen mit der Ereignisschleife
// funktionieren. QSignalSpy::wait() lässt dabei die Ereignisschleife laufen –
// im Test ist das in Ordnung, in der App nutzen wir nur Signale.
#include "network/PeerConnection.h"
#include "network/TcpService.h"
#include "network/UdpService.h"

#include <QSignalSpy>
#include <QTest>

class TestNetwork : public QObject
{
    Q_OBJECT

private slots:
    void tcpDeliversMessagesInBothDirections();
    void tcpReportsDisconnect();
    void tcpReportsRefusedConnection();
    void udpBroadcastReachesOthersButNotSelf();
    void udpIgnoresDuplicateDatagrams();
};

void TestNetwork::tcpDeliversMessagesInBothDirections()
{
    TcpService server;
    TcpService client;
    QVERIFY(server.listen(0));  // freier Port

    QSignalSpy serverConnected(&server, &TcpService::peerConnected);
    QSignalSpy clientConnected(&client, &TcpService::peerConnected);
    const int clientSideId = client.connectToPeer(QStringLiteral("127.0.0.1"), server.serverPort());

    QVERIFY(clientConnected.wait(3000));
    QTRY_COMPARE_WITH_TIMEOUT(serverConnected.count(), 1, 3000);
    const int serverSideId = serverConnected.at(0).at(0).toInt();

    // Client -> Server: mehrere Nachrichten direkt hintereinander, damit TCP
    // sie sehr wahrscheinlich zusammenklebt. Das Framing muss sie trennen.
    QSignalSpy serverReceived(&server, &TcpService::messageReceived);
    const QUuid id = QUuid::createUuid();
    for (int i = 0; i < 5; ++i)
        QVERIFY(client.send(clientSideId, Message::makeText(id, QStringLiteral("C"), QString::number(i))));

    QTRY_COMPARE_WITH_TIMEOUT(serverReceived.count(), 5, 3000);
    for (int i = 0; i < 5; ++i)
        QCOMPARE(serverReceived.at(i).at(1).value<Message>().text(), QString::number(i));

    // Server -> Client
    QSignalSpy clientReceived(&client, &TcpService::messageReceived);
    QVERIFY(server.send(serverSideId, Message::makeText(id, QStringLiteral("S"), QStringLiteral("Antwort"))));
    QVERIFY(clientReceived.wait(3000));
    QCOMPARE(clientReceived.at(0).at(1).value<Message>().text(), QStringLiteral("Antwort"));
}

void TestNetwork::tcpReportsDisconnect()
{
    TcpService server;
    TcpService client;
    QVERIFY(server.listen(0));

    QSignalSpy serverConnected(&server, &TcpService::peerConnected);
    QSignalSpy clientConnected(&client, &TcpService::peerConnected);
    const int clientSideId = client.connectToPeer(QStringLiteral("127.0.0.1"), server.serverPort());
    QVERIFY(clientConnected.wait(3000));
    QTRY_COMPARE_WITH_TIMEOUT(serverConnected.count(), 1, 3000);

    QSignalSpy serverDisconnected(&server, &TcpService::peerDisconnected);
    QSignalSpy errors(&server, &TcpService::errorOccurred);
    client.disconnectPeer(clientSideId);

    QVERIFY(serverDisconnected.wait(3000));
    // Normales Trennen ist kein Fehler (RemoteHostClosedError wird gefiltert).
    QCOMPARE(errors.count(), 0);
}

void TestNetwork::tcpReportsRefusedConnection()
{
    // Einen freien Port finden und wieder schließen -> dort lauscht niemand.
    quint16 unusedPort = 0;
    {
        TcpService probe;
        QVERIFY(probe.listen(0));
        unusedPort = probe.serverPort();
    }

    TcpService client;
    QSignalSpy errors(&client, &TcpService::errorOccurred);
    QSignalSpy disconnected(&client, &TcpService::peerDisconnected);
    QSignalSpy connected(&client, &TcpService::peerConnected);
    client.connectToPeer(QStringLiteral("127.0.0.1"), unusedPort);

    QVERIFY(disconnected.wait(PeerConnection::ConnectTimeoutMs + 1000));
    QVERIFY(errors.count() >= 1);
    QCOMPARE(connected.count(), 0);
}

void TestNetwork::udpBroadcastReachesOthersButNotSelf()
{
    const quint16 port = 45999;  // fester Port: beide müssen denselben teilen
    const QUuid idA = QUuid::createUuid();
    const QUuid idB = QUuid::createUuid();
    UdpService a(idA);
    UdpService b(idB);
    QVERIFY(a.start(port));
    QVERIFY(b.start(port));  // klappt nur dank ShareAddress | ReuseAddressHint

    QSignalSpy receivedByA(&a, &UdpService::messageReceived);
    QSignalSpy receivedByB(&b, &UdpService::messageReceived);
    QSignalSpy sendErrors(&a, &UdpService::errorOccurred);

    if (!a.sendBroadcast(Message::makeText(idA, QStringLiteral("A"), QStringLiteral("an alle"))))
        QSKIP(qPrintable(QStringLiteral("Kein Broadcast möglich (kein Netzwerk?): %1")
                             .arg(sendErrors.value(0).value(0).toString())));

    if (!receivedByB.wait(2000))
        QSKIP("Broadcast kam nicht an – Firewall oder Netzwerk ohne Broadcast. Siehe docs/testplan.md.");

    QCOMPARE(receivedByB.at(0).at(0).value<Message>().text(), QStringLiteral("an alle"));
    // A darf seine eigene Nachricht NICHT gemeldet bekommen (UUID-Filter).
    QTest::qWait(200);
    QCOMPARE(receivedByA.count(), 0);
}

void TestNetwork::udpIgnoresDuplicateDatagrams()
{
    // Hat ein Rechner mehrere Netzwerkkarten, schicken wir denselben Broadcast
    // über jede Karte – ein Empfänger kann ihn also mehrfach bekommen.
    // Simuliert: dieselbe Nachricht (identische Bytes) zweimal senden.
    const quint16 port = 45998;
    const QUuid idA = QUuid::createUuid();
    UdpService a(idA);
    UdpService b(QUuid::createUuid());
    QVERIFY(a.start(port));
    QVERIFY(b.start(port));

    QSignalSpy receivedByB(&b, &UdpService::messageReceived);
    const Message message = Message::makeText(idA, QStringLiteral("A"), QStringLiteral("einmal"));
    if (!a.sendBroadcast(message) || !a.sendBroadcast(message))
        QSKIP("Kein Broadcast möglich (kein Netzwerk?)");

    if (!receivedByB.wait(2000))
        QSKIP("Broadcast kam nicht an – Firewall oder Netzwerk ohne Broadcast.");
    QTest::qWait(300);  // dem zweiten Exemplar Zeit geben anzukommen
    QCOMPARE(receivedByB.count(), 1);
}

QTEST_GUILESS_MAIN(TestNetwork)
#include "tst_network.moc"

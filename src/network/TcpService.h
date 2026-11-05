#pragma once

#include "core/Message.h"

#include <QHash>
#include <QObject>

class PeerConnection;
class QTcpServer;
class QTcpSocket;

// Direkte Verbindungen per TCP.
//
// Jede Instanz ist gleichzeitig Server (QTcpServer nimmt eingehende
// Verbindungen an) und Client (connectToPeer baut ausgehende auf).
// Das ist der Kern von Peer-to-Peer: Es gibt keine feste Server-Rolle.
//
// Nach außen identifizieren wir Verbindungen über eine einfache Zahl
// (connectionId) statt über Zeiger. So kann niemand außerhalb versehentlich
// ein bereits gelöschtes Objekt benutzen.
class TcpService : public QObject
{
    Q_OBJECT

public:
    explicit TcpService(QObject* parent = nullptr);

    // Port 0 = "Betriebssystem, such einen freien Port aus".
    bool listen(quint16 port);
    quint16 serverPort() const;

    // Startet den Verbindungsaufbau und kehrt sofort zurück (nicht blockierend).
    int connectToPeer(const QString& host, quint16 port);

    bool send(int connectionId, const Message& message);
    void disconnectPeer(int connectionId);

signals:
    void peerConnected(int connectionId, const QString& address);
    void peerDisconnected(int connectionId);
    void messageReceived(int connectionId, const Message& message);
    void errorOccurred(const QString& description);

private slots:
    void onNewConnection();

private:
    PeerConnection* addConnection(QTcpSocket* socket);

    QTcpServer* m_server;
    QHash<int, PeerConnection*> m_connections;
    int m_nextId = 1;
};

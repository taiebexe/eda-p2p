#include "network/TcpService.h"

#include "network/PeerConnection.h"

#include <QTcpServer>
#include <QTcpSocket>

TcpService::TcpService(QObject* parent)
    : QObject(parent)
    , m_server(new QTcpServer(this))
{
    connect(m_server, &QTcpServer::newConnection, this, &TcpService::onNewConnection);
}

bool TcpService::listen(quint16 port)
{
    // QHostAddress::Any: Verbindungen über IPv4 und IPv6, auf allen Netzwerkkarten.
    if (!m_server->listen(QHostAddress::Any, port)) {
        emit errorOccurred(tr("TCP-Port %1 konnte nicht geöffnet werden: %2")
                               .arg(port)
                               .arg(m_server->errorString()));
        return false;
    }
    return true;
}

quint16 TcpService::serverPort() const
{
    return m_server->serverPort();
}

int TcpService::connectToPeer(const QString& host, quint16 port)
{
    PeerConnection* connection = addConnection(new QTcpSocket);
    const int id = m_connections.key(connection);
    connection->connectToHost(host, port);
    return id;
}

bool TcpService::send(int connectionId, const Message& message)
{
    PeerConnection* connection = m_connections.value(connectionId, nullptr);
    return connection != nullptr && connection->send(message);
}

void TcpService::disconnectPeer(int connectionId)
{
    if (PeerConnection* connection = m_connections.value(connectionId, nullptr))
        connection->close();
}

void TcpService::onNewConnection()
{
    // Auch hier eine Schleife: Mehrere Verbindungen können gleichzeitig warten.
    while (m_server->hasPendingConnections()) {
        PeerConnection* connection = addConnection(m_server->nextPendingConnection());
        // Eingehende Verbindungen stehen bereits – sofort melden.
        emit peerConnected(m_connections.key(connection), connection->address());
    }
}

PeerConnection* TcpService::addConnection(QTcpSocket* socket)
{
    const int id = m_nextId++;
    auto* connection = new PeerConnection(socket, this);
    m_connections.insert(id, connection);

    // Die Signale der einzelnen Verbindung werden mit der ID "angereichert"
    // weitergereicht. Der Aufrufer muss PeerConnection gar nicht kennen.
    connect(connection, &PeerConnection::established, this, [this, id, connection] {
        emit peerConnected(id, connection->address());
    });
    connect(connection, &PeerConnection::messageReceived, this, [this, id](const Message& message) {
        emit messageReceived(id, message);
    });
    connect(connection, &PeerConnection::errorOccurred, this, &TcpService::errorOccurred);
    connect(connection, &PeerConnection::closed, this, [this, id, connection] {
        m_connections.remove(id);
        emit peerDisconnected(id);
        // deleteLater statt delete: Wir sind gerade mitten in einem Signal
        // DIESES Objekts. Sofortiges Löschen würde den Boden unter den Füßen
        // wegziehen. deleteLater löscht, sobald die Ereignisschleife wieder dran ist.
        connection->deleteLater();
    });
    return connection;
}

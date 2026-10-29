#include "TcpPeer.h"
#include "Message.h"

#include <utility>

TcpPeer::TcpPeer(QString sender, QObject* parent) : QObject(parent), m_sender(std::move(sender))
{
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (m_server.hasPendingConnections()) {
            auto* socket = m_server.nextPendingConnection();
            if (m_socket) {
                socket->abort();
                socket->deleteLater();
                continue;
            }
            attachSocket(socket);
        }
    });
}

bool TcpPeer::listen(quint16 port)
{
    if (!m_server.listen(QHostAddress::AnyIPv4, port)) {
        emit errorOccurred("TCP listen: " + m_server.errorString());
        return false;
    }
    emit listeningChanged();
    return true;
}

bool TcpPeer::isConnected() const
{
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void TcpPeer::connectToPeer(const QString& host, int port)
{
    if (host.trimmed().isEmpty() || port < 1 || port > 65535) {
        emit errorOccurred("Enter a host and a TCP port between 1 and 65535.");
        return;
    }
    disconnectPeer();
    attachSocket(new QTcpSocket(this));
    m_socket->connectToHost(host.trimmed(), static_cast<quint16>(port));
}

void TcpPeer::disconnectPeer()
{
    if (!m_socket)
        return;
    m_socket->disconnect(this);
    m_socket->abort();
    m_socket->deleteLater();
    m_socket = nullptr;
    m_buffer.clear();
    emit connectionChanged();
}

void TcpPeer::attachSocket(QTcpSocket* socket)
{
    m_socket = socket;
    socket->setParent(this);
    socket->setReadBufferSize(Message::MaxBytes + 1);
    connect(socket, &QTcpSocket::connected, this, &TcpPeer::connectionChanged);
    connect(socket, &QTcpSocket::readyRead, this, &TcpPeer::readLines);
    connect(socket, &QTcpSocket::disconnected, this, &TcpPeer::disconnectPeer);
    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        if (error == QAbstractSocket::RemoteHostClosedError)
            return; // Normal peer shutdown is handled by disconnected().
        emit errorOccurred("TCP: " + m_socket->errorString());
        disconnectPeer();
    });
    emit connectionChanged();
    // An accepted socket might already have buffered data.
    if (socket->bytesAvailable() > 0)
        readLines();
}

bool TcpPeer::sendText(const QString& text)
{
    if (!isConnected() || !Message::validText(text)) {
        emit errorOccurred("Connect a TCP peer and enter 1 to 1000 characters.");
        return false;
    }
    const auto line = Message::encode("text", m_sender, {{"text", text}}) + '\n';
    if (m_socket->write(line) != line.size()) {
        emit errorOccurred("TCP send: " + m_socket->errorString());
        return false;
    }
    return true;
}

void TcpPeer::readLines()
{
    m_buffer += m_socket->readAll();
    qsizetype newline = -1;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const auto message = Message::decode(m_buffer.left(newline));
        m_buffer.remove(0, newline + 1);
        if (!message || (*message)["type"].toString() != "text")
            continue;
        const QString text = (*message)["payload"].toObject()["text"].toString();
        if (Message::validText(text))
            emit messageReceived((*message)["sender"].toString(), text);
    }
    if (m_buffer.size() > Message::MaxBytes) {
        emit errorOccurred("TCP frame exceeds 8192 bytes.");
        disconnectPeer();
    }
}

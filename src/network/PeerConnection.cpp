#include "network/PeerConnection.h"

#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>

namespace {
// Ein Server auf "Any" (IPv4+IPv6) meldet IPv4-Gegenüber als "::ffff:192.168.0.42".
// Für die Anzeige wandeln wir das in normales IPv4 um.
QString readableAddress(const QHostAddress& address)
{
    bool isIPv4 = false;
    const quint32 ipv4 = address.toIPv4Address(&isIPv4);
    return isIPv4 ? QHostAddress(ipv4).toString() : address.toString();
}
} // namespace

PeerConnection::PeerConnection(QTcpSocket* socket, QObject* parent)
    : QObject(parent)
    , m_socket(socket)
{
    m_socket->setParent(this);  // Lebensdauer: Socket stirbt mit der Verbindung

    connect(m_socket, &QTcpSocket::connected, this, &PeerConnection::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &PeerConnection::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &PeerConnection::onSocketError);
    // disconnected kommt nur, wenn die Verbindung vorher stand.
    connect(m_socket, &QTcpSocket::disconnected, this, &PeerConnection::markClosed);
}

void PeerConnection::connectToHost(const QString& host, quint16 port)
{
    m_socket->connectToHost(host, port);

    // Zeitlimit über die Ereignisschleife statt waitForConnected(): Die
    // Oberfläche bleibt bedienbar. "this" als Kontextobjekt sorgt dafür, dass
    // das Lambda nicht mehr läuft, falls die Verbindung vorher gelöscht wurde.
    QTimer::singleShot(ConnectTimeoutMs, this, [this] {
        if (m_socket->state() != QAbstractSocket::ConnectedState && !m_closed) {
            emit errorOccurred(tr("Zeitüberschreitung beim Verbinden mit %1:%2")
                                   .arg(m_socket->peerName())
                                   .arg(m_socket->peerPort()));
            m_socket->abort();
            markClosed();
        }
    });
}

bool PeerConnection::send(const Message& message)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
        return false;

    const QByteArray frame = FrameCodec::encode(message.toJson());
    if (frame.isEmpty())
        return false;

    // write() legt die Daten in Qts Sendepuffer und kehrt sofort zurück.
    // Das eigentliche Senden erledigt die Ereignisschleife im Hintergrund.
    return m_socket->write(frame) == frame.size();
}

void PeerConnection::close()
{
    // disconnectFromHost sendet noch ausstehende Daten und schließt dann sauber.
    m_socket->disconnectFromHost();
    // War der Socket nie verbunden, kommt kein disconnected() – selbst melden.
    if (m_socket->state() == QAbstractSocket::UnconnectedState)
        markClosed();
}

QString PeerConnection::address() const
{
    return QStringLiteral("%1:%2").arg(readableAddress(m_socket->peerAddress())).arg(m_socket->peerPort());
}

void PeerConnection::onConnected()
{
    emit established();
}

void PeerConnection::onReadyRead()
{
    // Alles Verfügbare in den Puffer – egal ob halbe, ganze oder mehrere Nachrichten.
    m_codec.append(m_socket->readAll());

    QByteArray frame;
    for (;;) {
        switch (m_codec.next(frame)) {
        case FrameCodec::Result::NeedMoreData:
            return;  // Rest kommt mit dem nächsten readyRead
        case FrameCodec::Result::Error:
            emit errorOccurred(tr("Ungültige Daten von %1 – Verbindung wird getrennt.").arg(address()));
            m_socket->abort();
            markClosed();
            return;
        case FrameCodec::Result::Frame:
            if (const std::optional<Message> message = Message::fromJson(frame))
                emit messageReceived(*message);
            // Ungültiges JSON in einem korrekten Frame: nur diese Nachricht verwerfen.
            break;
        }
    }
}

void PeerConnection::onSocketError(QAbstractSocket::SocketError error)
{
    // Wenn der andere die Verbindung schließt, meldet Qt das auch als "Fehler".
    // Für uns ist das ein normales Ende, keine Fehlermeldung wert.
    if (error != QAbstractSocket::RemoteHostClosedError)
        emit errorOccurred(m_socket->errorString());

    // Fehlgeschlagener Verbindungsaufbau (z. B. "Connection refused"): Der
    // Socket war nie verbunden, also kommt kein disconnected().
    if (m_socket->state() == QAbstractSocket::UnconnectedState)
        markClosed();
}

void PeerConnection::markClosed()
{
    // Mehrere Wege führen zum Ende (Fehler, disconnected, Timeout).
    // Der Besitzer soll closed() aber genau einmal bekommen.
    if (m_closed)
        return;
    m_closed = true;
    emit closed();
}

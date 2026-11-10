#include "ChatController.h"

#include "network/TcpService.h"
#include "network/UdpService.h"

#include <QHostAddress>
#include <QNetworkInterface>

ChatController::ChatController(const QString& displayName, quint16 udpPort, quint16 tcpPort,
                               QObject* parent)
    : QObject(parent)
    , m_ownId(QUuid::createUuid())
    , m_displayName(displayName.trimmed().isEmpty() ? tr("Anonym") : displayName.trimmed())
    , m_requestedUdpPort(udpPort)
    , m_requestedTcpPort(tcpPort)
    , m_udp(new UdpService(m_ownId, this))
    , m_tcp(new TcpService(this))
{
    //!
    // Alle Verbindungen zwischen Netzwerk und Oberfläche laufen über Signale.
    // Die Services wissen nicht, wer zuhört – lose Kopplung.
    connect(m_udp, &UdpService::messageReceived, this,
            [this](const Message& message, const QHostAddress&) { onUdpMessage(message); });
    connect(m_udp, &UdpService::errorOccurred, this, &ChatController::logSystem);

    connect(m_tcp, &TcpService::peerConnected, this, &ChatController::onTcpConnected);
    connect(m_tcp, &TcpService::peerDisconnected, this, &ChatController::onTcpDisconnected);
    connect(m_tcp, &TcpService::messageReceived, this, &ChatController::onTcpMessage);
    connect(m_tcp, &TcpService::errorOccurred, this, &ChatController::logSystem);
}

void ChatController::start()
{
    if (m_udp->start(m_requestedUdpPort)) {
        // Zeigen, über welche Netze wir senden – hilft bei der Fehlersuche,
        // wenn z. B. ein VPN- oder VirtualBox-Adapter statt des WLANs aktiv ist.
        QStringList targets;
        for (const QHostAddress& address : UdpService::broadcastAddresses())
            targets.append(address.toString());
        logSystem(tr("UDP-Broadcast aktiv auf Port %1, sende an: %2.")
                      .arg(m_udp->port())
                      .arg(targets.isEmpty() ? tr("255.255.255.255 (kein Netzwerk gefunden)")
                                             : targets.join(QStringLiteral(", "))));
    }

    // Ist der TCP-Port belegt (z. B. zweite Instanz ohne --tcp-port), lassen
    // wir das Betriebssystem einen freien Port wählen und zeigen ihn an.
    // So funktioniert der Test mit zwei Instanzen auch ohne Parameter.
    if (!m_tcp->listen(m_requestedTcpPort) && m_requestedTcpPort != 0) {
        if (m_tcp->listen(0))
            logSystem(tr("Ausweich-Port gewählt."));
    }
    if (m_tcp->serverPort() != 0)
        logSystem(tr("Warte auf TCP-Verbindungen auf Port %1.").arg(m_tcp->serverPort()));

    emit portsChanged();
}

void ChatController::setDisplayName(const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty() || trimmed == m_displayName)
        return;
    m_displayName = trimmed;
    emit displayNameChanged();
}

int ChatController::udpPort() const
{
    return m_udp->port();
}

int ChatController::tcpPort() const
{
    return m_tcp->serverPort();
}

QString ChatController::localAddresses() const
{
    // Zeigt die eigenen IPv4-Adressen an, damit man weiß, was der andere
    // ins Feld "IP:Port" eintippen muss.
    QStringList result;
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback())
            result << address.toString();
    }
    return result.isEmpty() ? QStringLiteral("127.0.0.1") : result.join(QStringLiteral(", "));
}

QStringList ChatController::peers() const
{
    QStringList result;
    for (const PeerInfo& peer : m_peers)
        result << (peer.name.isEmpty() ? peer.address
                                       : QStringLiteral("%1 (%2)").arg(peer.name, peer.address));
    return result;
}

bool ChatController::sendBroadcast(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return false;

    const Message message = Message::makeText(m_ownId, m_displayName, trimmed);
    if (!m_udp->sendBroadcast(message))
        return false;

    // Eigene Nachrichten trägt der Controller selbst ein – der UdpService
    // filtert das Echo ja heraus.
    m_chatLog.append({message.timestamp, m_displayName, trimmed,
                      ChatLogModel::Channel::Broadcast, true});
    return true;
}

bool ChatController::connectToPeer(const QString& hostAndPort)
{
    // Erwartet "Host:Port". lastIndexOf, weil der Port immer hinten steht.
    const QString input = hostAndPort.trimmed();
    const int colon = static_cast<int>(input.lastIndexOf(QLatin1Char(':')));
    bool portOk = false;
    const QString host = colon > 0 ? input.left(colon) : QString();
    const uint port = colon > 0 ? input.mid(colon + 1).toUInt(&portOk) : 0;

    if (host.isEmpty() || !portOk || port == 0 || port > 65535) {
        logSystem(tr("Bitte im Format IP:Port eingeben, z. B. 192.168.0.42:45455"));
        return false;
    }

    logSystem(tr("Verbinde mit %1:%2 …").arg(host).arg(port));
    // Kehrt sofort zurück. Erfolg oder Fehler kommen später als Signal.
    m_tcp->connectToPeer(host, static_cast<quint16>(port));
    return true;
}

bool ChatController::sendDirect(int peerIndex, const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || peerIndex < 0 || peerIndex >= m_peers.size())
        return false;

    const PeerInfo& peer = m_peers.at(peerIndex);
    const Message message = Message::makeText(m_ownId, m_displayName, trimmed);
    if (!m_tcp->send(peer.connectionId, message)) {
        logSystem(tr("Senden an %1 fehlgeschlagen.").arg(peer.address));
        return false;
    }

    const QString target = peer.name.isEmpty() ? peer.address : peer.name;
    m_chatLog.append({message.timestamp, tr("%1 → %2").arg(m_displayName, target), trimmed,
                      ChatLogModel::Channel::Direct, true});
    return true;
}

void ChatController::onUdpMessage(const Message& message)
{
    if (message.type == MessageType::Text) {
        m_chatLog.append({message.timestamp, message.senderName, message.text(),
                          ChatLogModel::Channel::Broadcast, false});
    }
    // Unbekannte Typen ignorieren: Neuere Versionen dürfen neue Typen senden,
    // ohne dass ältere abstürzen. (Hier landet später z. B. "profile".)
}

void ChatController::onTcpConnected(int connectionId, const QString& address)
{
    m_peers.append({connectionId, address, QString()});
    emit peersChanged();
    logSystem(tr("TCP-Verbindung mit %1 hergestellt.").arg(address));

    // Beide Seiten stellen sich sofort vor. So kennt jede Seite den Namen
    // des anderen, ohne dass vorher eine Nachricht geschrieben werden muss.
    m_tcp->send(connectionId, Message::makeHello(m_ownId, m_displayName));
}

void ChatController::onTcpDisconnected(int connectionId)
{
    const int index = indexOfPeer(connectionId);
    if (index < 0)
        return;  // Verbindungsaufbau gescheitert – war nie in der Liste
    const PeerInfo peer = m_peers.takeAt(index);
    emit peersChanged();
    logSystem(tr("Verbindung mit %1 getrennt.").arg(peer.name.isEmpty() ? peer.address : peer.name));
}

void ChatController::onTcpMessage(int connectionId, const Message& message)
{
    const int index = indexOfPeer(connectionId);
    if (index < 0)
        return;

    if (message.type == MessageType::Hello) {
        // Mit uns selbst verbunden (eigene IP + eigener Port eingegeben)?
        if (message.senderId == m_ownId) {
            logSystem(tr("Das ist die eigene Instanz – Verbindung wird getrennt."));
            m_tcp->disconnectPeer(connectionId);
            return;
        }
        m_peers[index].name = message.senderName;
        emit peersChanged();
        logSystem(tr("%1 ist jetzt direkt verbunden.").arg(message.senderName));
    } else if (message.type == MessageType::Text) {
        m_chatLog.append({message.timestamp, message.senderName, message.text(),
                          ChatLogModel::Channel::Direct, false});
    }
}

void ChatController::logSystem(const QString& text)
{
    m_chatLog.append({QDateTime::currentDateTimeUtc(), tr("System"), text,
                      ChatLogModel::Channel::System, false});
}

int ChatController::indexOfPeer(int connectionId) const
{
    for (int i = 0; i < m_peers.size(); ++i) {
        if (m_peers.at(i).connectionId == connectionId)
            return i;
    }
    return -1;
}

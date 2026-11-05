#include "network/UdpService.h"

#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QStringList>
#include <QUdpSocket>

UdpService::UdpService(const QUuid& ownId, QObject* parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))  // this als Parent -> keine manuelle delete nötig
    , m_ownId(ownId)
{
    // readyRead wird von der Qt-Ereignisschleife ausgelöst, sobald Daten da sind.
    // Kein Polling, kein eigener Thread, kein Blockieren der Oberfläche.
    connect(m_socket, &QUdpSocket::readyRead, this, &UdpService::onReadyRead);
}

bool UdpService::start(quint16 port)
{
    // AnyIPv4: Broadcast gibt es nur bei IPv4 (IPv6 kennt nur Multicast).
    //
    // ShareAddress | ReuseAddressHint: Mehrere Programme dürfen denselben
    // UDP-Port binden. Nur so laufen zwei Instanzen auf EINEM Rechner, und
    // beide bekommen jeden Broadcast.
    const bool ok = m_socket->bind(QHostAddress::AnyIPv4, port,
                                   QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    if (!ok) {
        emit errorOccurred(tr("UDP-Port %1 konnte nicht geöffnet werden: %2")
                               .arg(port)
                               .arg(m_socket->errorString()));
        return false;
    }
    m_port = m_socket->localPort();
    return true;
}

bool UdpService::sendBroadcast(const Message& message)
{
    if (m_port == 0) {
        emit errorOccurred(tr("UDP ist nicht gestartet."));
        return false;
    }

    const QByteArray data = message.toJson();
    if (data.size() > MaxDatagramSize) {
        emit errorOccurred(tr("Nachricht ist zu groß für einen Broadcast (%1 Bytes).").arg(data.size()));
        return false;
    }

    // WARUM NICHT EINFACH 255.255.255.255 (QHostAddress::Broadcast)?
    // Diese "limitierte" Broadcast-Adresse gehört zu keinem Netz. Das
    // Betriebssystem muss raten, über welche Karte sie raus soll:
    //  - macOS antwortet darauf zeitweise mit "No route to host" (bei uns
    //    gemessen: dieselbe Zeile klappt mal, mal minutenlang nicht),
    //  - Windows schickt sie nur über EINE Karte, evtl. die falsche
    //    (z. B. VirtualBox- oder VPN-Adapter statt WLAN).
    // Die Broadcast-Adresse eines konkreten Netzes (z. B. 10.1.255.255 für
    // 10.1.x.x/16) hat dagegen eine eindeutige Route. Deshalb senden wir an
    // die Broadcast-Adresse jeder aktiven Karte.
    QList<QHostAddress> targets = broadcastAddresses();
    if (targets.isEmpty())
        targets.append(QHostAddress::Broadcast);  // keine Karte gefunden: letzter Versuch

    // Ziel-Port ist derselbe Port, auf dem wir selbst lauschen: Alle Instanzen
    // teilen sich einen "Kanal"-Port. Das Datagramm kommt auch bei uns selbst
    // an – das filtern wir in onReadyRead() über die senderId heraus.
    int sentCount = 0;
    QStringList failures;
    for (const QHostAddress& target : targets) {
        if (m_socket->writeDatagram(data, target, m_port) == data.size())
            ++sentCount;
        else
            failures.append(QStringLiteral("%1: %2").arg(target.toString(), m_socket->errorString()));
    }

    // Eine funktionierende Karte reicht. Fehler an anderen Karten (z. B. ein
    // abgeschalteter Adapter) sind dann kein Problem für den Nutzer.
    if (sentCount == 0) {
        emit errorOccurred(tr("Broadcast fehlgeschlagen (%1)").arg(failures.join(QStringLiteral("; "))));
        return false;
    }
    return true;
}

QList<QHostAddress> UdpService::broadcastAddresses()
{
    QList<QHostAddress> result;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        const QNetworkInterface::InterfaceFlags flags = iface.flags();
        // Nur Karten, die an sind, Verbindung haben und Broadcast können.
        // Loopback (127.0.0.1) und VPN-Tunnel (Punkt-zu-Punkt) fallen so raus.
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || !flags.testFlag(QNetworkInterface::CanBroadcast) || flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;

        const QList<QNetworkAddressEntry> entries = iface.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            const QHostAddress broadcast = entry.broadcast();  // bei IPv6 leer
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && !broadcast.isNull()
                && !result.contains(broadcast))
                result.append(broadcast);
        }
    }
    return result;
}

bool UdpService::isDuplicate(const QByteArray& data)
{
    if (m_recentDatagrams.contains(data))
        return true;
    m_recentDatagrams.append(data);
    if (m_recentDatagrams.size() > RecentDatagramCount)
        m_recentDatagrams.removeFirst();  // Liste klein halten: nur die letzten 32
    return false;
}

void UdpService::onReadyRead()
{
    // WHILE statt IF: readyRead kommt nur einmal, auch wenn mehrere Datagramme
    // warten. Lesen wir nur eins, bleiben die anderen liegen, bis zufällig ein
    // neues Paket eintrifft.
    while (m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();

        // Dieselbe Nachricht über eine zweite Netzwerkkarte: schon gezeigt.
        if (isDuplicate(datagram.data()))
            continue;

        const std::optional<Message> message = Message::fromJson(datagram.data());
        if (!message)
            continue;  // fremdes oder kaputtes Paket: still ignorieren

        // Eigene Broadcasts kommen zurück. Die IP-Adresse taugt nicht als
        // Erkennung (zwei Instanzen auf einem PC haben dieselbe), die UUID schon.
        if (message->senderId == m_ownId)
            continue;

        emit messageReceived(*message, datagram.senderAddress());
    }
}

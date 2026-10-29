#include "UdpDiscovery.h"
#include "Message.h"

#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QSet>
#include <utility>

UdpDiscovery::UdpDiscovery(QString sender, QObject* parent)
    : QObject(parent), m_sender(std::move(sender))
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &UdpDiscovery::readDatagrams);
    m_timer.setInterval(3000);
    connect(&m_timer, &QTimer::timeout, this, [this] { broadcast("hello"); });
}

bool UdpDiscovery::start(quint16 tcpPort)
{
    m_tcpPort = tcpPort;
    if (!m_socket.bind(QHostAddress::AnyIPv4, Port,
                       QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        emit errorOccurred("UDP bind: " + m_socket.errorString());
        return false;
    }
    m_timer.start();
    broadcast("hello");
    return true;
}

bool UdpDiscovery::sendText(const QString& text)
{
    if (!Message::validText(text)) {
        emit errorOccurred("Enter 1 to 1000 characters of text.");
        return false;
    }
    return broadcast("text", text);
}

bool UdpDiscovery::broadcast(const QString& type, const QString& text)
{
    const auto data = Message::encode(type, m_sender, {{"text", text}, {"tcpPort", m_tcpPort}});
    // Send on each active IPv4 subnet, including when a VPN changes the default route.
    QSet<QHostAddress> destinations;
    for (const auto& interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning) ||
            flags.testFlag(QNetworkInterface::IsLoopBack))
            continue;
        for (const auto& entry : interface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                !entry.broadcast().isNull())
                destinations.insert(entry.broadcast());
        }
    }
    if (destinations.isEmpty()) {
        emit errorOccurred("UDP: no active IPv4 LAN interface. Connect to Wi-Fi or Ethernet.");
        return false;
    }
    bool sent = false;
    for (const auto& destination : destinations)
        sent = (m_socket.writeDatagram(data, destination, Port) == data.size()) || sent;
    if (!sent)
        emit errorOccurred("UDP send: " + m_socket.errorString());
    return sent;
}

void UdpDiscovery::readDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        const auto datagram = m_socket.receiveDatagram(Message::MaxBytes + 1);
        const auto message = Message::decode(datagram.data());
        if (!message || (*message)["sender"].toString() == m_sender)
            continue;
        const QString type = (*message)["type"].toString();
        if (type != "hello" && type != "text")
            continue;
        const auto payload = (*message)["payload"].toObject();
        const int port = payload["tcpPort"].toInt();
        if (port < 1 || port > 65535)
            continue;
        const QString sender = (*message)["sender"].toString();
        emit peerDiscovered(sender, datagram.senderAddress().toString(), port);
        const QString text = payload["text"].toString();
        if (type == "text" && Message::validText(text))
            emit messageReceived(sender, text);
    }
}

#pragma once

#include "ChatLogModel.h"
#include "core/Message.h"

#include <QObject>
#include <QStringList>
#include <QUuid>
#include <QtQml/qqmlregistration.h>

class TcpService;
class UdpService;

// Die Brücke zwischen QML-Oberfläche und Netzwerk.
//
// QML sieht nur:  Properties  (Daten, die sich automatisch aktualisieren)
//                 Q_INVOKABLE (Methoden, die QML aufrufen darf)
// Die Netzwerk-Services kennen QML nicht – sie sind austauschbar und
// ohne Oberfläche testbar.
class ChatController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ChatController wird in main.cpp erzeugt.")

    // NOTIFY-Signal: Ändert sich der Wert, aktualisiert QML alle Bindungen.
    Q_PROPERTY(QString displayName READ displayName WRITE setDisplayName NOTIFY displayNameChanged)
    Q_PROPERTY(int udpPort READ udpPort NOTIFY portsChanged)
    Q_PROPERTY(int tcpPort READ tcpPort NOTIFY portsChanged)
    Q_PROPERTY(QString localAddresses READ localAddresses CONSTANT)
    Q_PROPERTY(QStringList peers READ peers NOTIFY peersChanged)
    Q_PROPERTY(ChatLogModel* chatLog READ chatLog CONSTANT)

public:
    ChatController(const QString& displayName, quint16 udpPort, quint16 tcpPort,
                   QObject* parent = nullptr);

    // Öffnet die Ports. Getrennt vom Konstruktor, damit Fehler im Chatlog landen.
    void start();

    QString displayName() const { return m_displayName; }
    void setDisplayName(const QString& name);
    int udpPort() const;
    int tcpPort() const;
    QString localAddresses() const;
    QStringList peers() const;
    ChatLogModel* chatLog() { return &m_chatLog; }

    // Rückgabe bool: QML leert das Eingabefeld nur, wenn wirklich gesendet wurde.
    Q_INVOKABLE bool sendBroadcast(const QString& text);
    Q_INVOKABLE bool connectToPeer(const QString& hostAndPort);
    Q_INVOKABLE bool sendDirect(int peerIndex, const QString& text);

signals:
    void displayNameChanged();
    void portsChanged();
    void peersChanged();

private:
    // Was wir über eine TCP-Verbindung wissen. Der Name kommt per "hello".
    struct PeerInfo
    {
        int connectionId = 0;
        QString address;
        QString name;
    };

    void onUdpMessage(const Message& message);
    void onTcpConnected(int connectionId, const QString& address);
    void onTcpDisconnected(int connectionId);
    void onTcpMessage(int connectionId, const Message& message);

    void logSystem(const QString& text);
    int indexOfPeer(int connectionId) const;

    QUuid m_ownId;  // unsere Peer-ID, pro Programmstart neu
    QString m_displayName;
    quint16 m_requestedUdpPort;
    quint16 m_requestedTcpPort;

    UdpService* m_udp;
    TcpService* m_tcp;
    ChatLogModel m_chatLog;
    QList<PeerInfo> m_peers;
};

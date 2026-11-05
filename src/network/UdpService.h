#pragma once

#include "core/Message.h"

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QUuid>

class QUdpSocket;

// Broadcast-Chat über UDP: eine Nachricht geht an alle Rechner im Subnetz.
//
// UDP ist verbindungslos: Es gibt keine "Peers", nur einzelne Datagramme.
// Ein Datagramm kommt ganz oder gar nicht an – deshalb braucht UDP (anders
// als TCP) kein Framing. Dafür gibt es keine Zustellgarantie.
class UdpService : public QObject
{
    Q_OBJECT

public:
    // Größte Nachricht, die wir per UDP senden. Theoretisch gehen ~64 KB,
    // aber große Datagramme werden in IP-Fragmente zerlegt; geht ein Fragment
    // verloren, ist das ganze Datagramm weg. Für Chattext reicht 8 KB locker.
    static constexpr int MaxDatagramSize = 8 * 1024;

    // ownId: unsere eigene Peer-ID, um eigene Broadcasts zu erkennen.
    explicit UdpService(const QUuid& ownId, QObject* parent = nullptr);

    // Bindet den Port. false + errorOccurred bei Fehler.
    bool start(quint16 port);

    // Sendet an die Broadcast-Adresse JEDER aktiven Netzwerkkarte
    // (z. B. 192.168.0.255). true, wenn es über mindestens eine Karte klappt.
    bool sendBroadcast(const Message& message);

    // Die Broadcast-Adressen aller aktiven IPv4-Netzwerkkarten, ohne Duplikate.
    static QList<QHostAddress> broadcastAddresses();

    quint16 port() const { return m_port; }

signals:
    void messageReceived(const Message& message, const QHostAddress& from);
    void errorOccurred(const QString& description);

private slots:
    void onReadyRead();

private:
    QUdpSocket* m_socket;  // Kind-Objekt: Qt löscht es automatisch mit uns
    // Kommt dieselbe Nachricht über zwei Netzwerkkarten doppelt an, sind die
    // Bytes identisch (gleiche senderId, gleicher Zeitstempel auf die
    // Millisekunde). Die letzten paar Datagramme merken wir uns zum Vergleich.
    bool isDuplicate(const QByteArray& data);
    static constexpr int RecentDatagramCount = 32;

    QUuid m_ownId;
    quint16 m_port = 0;
    QList<QByteArray> m_recentDatagrams;
};

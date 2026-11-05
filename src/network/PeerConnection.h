#pragma once

#include "core/FrameCodec.h"
#include "core/Message.h"

#include <QAbstractSocket>
#include <QObject>

class QTcpSocket;

// Eine TCP-Verbindung zu genau einem anderen Peer – egal, ob wir sie
// aufgebaut haben (ausgehend) oder der andere (eingehend).
//
// Warum eine eigene Klasse? Jede Verbindung braucht ihren eigenen
// Empfangspuffer für das Framing. Später kommt hier auch das Profil
// des Gegenübers hin.
class PeerConnection : public QObject
{
    Q_OBJECT

public:
    // Wie lange wir beim Verbindungsaufbau warten. Ohne eigenes Limit wartet
    // das Betriebssystem bei einer nicht erreichbaren IP bis zu ~75 Sekunden.
    static constexpr int ConnectTimeoutMs = 5000;

    // Übernimmt den Socket (setzt sich selbst als Parent).
    explicit PeerConnection(QTcpSocket* socket, QObject* parent = nullptr);

    // Startet eine ausgehende Verbindung. Kehrt sofort zurück; das Ergebnis
    // kommt später als Signal established() oder errorOccurred()+closed().
    void connectToHost(const QString& host, quint16 port);

    bool send(const Message& message);
    void close();

    // "192.168.0.42:45455" – zum Anzeigen.
    QString address() const;

signals:
    void established();                          // Verbindung steht
    void messageReceived(const Message& message);
    void errorOccurred(const QString& description);
    void closed();                               // endgültig vorbei (genau einmal)

private slots:
    void onConnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void markClosed();

    QTcpSocket* m_socket;
    FrameCodec m_codec;
    bool m_closed = false;
};

#pragma once

#include <QObject>
#include <QTimer>
#include <QUdpSocket>

class UdpDiscovery : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString sender READ sender CONSTANT)

public:
    static constexpr quint16 Port = 45454;
    explicit UdpDiscovery(QString sender, QObject* parent = nullptr);
    QString sender() const { return m_sender; }
    bool start(quint16 tcpPort);
    Q_INVOKABLE bool sendText(const QString& text);

signals:
    void peerDiscovered(const QString& sender, const QString& address, int tcpPort);
    void messageReceived(const QString& sender, const QString& text);
    void errorOccurred(const QString& message);

private:
    bool broadcast(const QString& type, const QString& text = {});
    void readDatagrams();
    QString m_sender;
    QUdpSocket m_socket;
    QTimer m_timer;
    quint16 m_tcpPort = 0;
};

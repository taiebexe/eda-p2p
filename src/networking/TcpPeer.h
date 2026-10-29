#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

// A small server and one direct connection. Everything runs on Qt's event loop.
class TcpPeer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int port READ port NOTIFY listeningChanged)
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectionChanged)

public:
    explicit TcpPeer(QString sender, QObject* parent = nullptr);
    bool listen(quint16 port = 0);
    int port() const { return m_server.serverPort(); }
    bool isConnected() const;
    Q_INVOKABLE void connectToPeer(const QString& host, int port);
    Q_INVOKABLE bool sendText(const QString& text);
    Q_INVOKABLE void disconnectPeer();

signals:
    void listeningChanged();
    void connectionChanged();
    void messageReceived(const QString& sender, const QString& text);
    void errorOccurred(const QString& message);

private:
    void attachSocket(QTcpSocket* socket);
    void readLines();
    QString m_sender;
    QTcpServer m_server;
    QTcpSocket* m_socket = nullptr;
    QByteArray m_buffer;
};

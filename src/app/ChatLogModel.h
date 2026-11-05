#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QtQml/qqmlregistration.h>

// Der Chatverlauf als Listenmodell für die QML-ListView.
//
// Warum ein Modell statt z. B. einer QStringList? Bei einem Modell meldet
// beginInsertRows()/endInsertRows() der View genau "eine Zeile ist neu" –
// die ListView zeichnet nur diese eine Zeile neu, statt die ganze Liste.
// Außerdem hat jede Zeile mehrere benannte Felder (Rollen), die QML direkt
// als Eigenschaften im Delegate benutzen kann.
class ChatLogModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("ChatLogModel wird vom ChatController bereitgestellt.")

public:
    enum class Channel { Broadcast, Direct, System };
    Q_ENUM(Channel)

    struct Entry
    {
        QDateTime timestamp;
        QString senderName;
        QString text;
        Channel channel = Channel::System;
        bool outgoing = false;  // von uns gesendet?
    };

    // Rollen = Spaltennamen, unter denen QML die Felder sieht.
    enum Role {
        TimeRole = Qt::UserRole + 1,
        SenderNameRole,
        MessageTextRole,
        ChannelRole,
        OutgoingRole,
    };

    using QAbstractListModel::QAbstractListModel;  // Konstruktor erben

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void append(const Entry& entry);

private:
    QList<Entry> m_entries;
};

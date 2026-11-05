#include "ChatLogModel.h"

int ChatLogModel::rowCount(const QModelIndex& parent) const
{
    // Eine Liste hat keine Kind-Elemente: Für einen gültigen parent gibt es 0 Zeilen.
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant ChatLogModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry& entry = m_entries.at(index.row());
    switch (role) {
    case TimeRole:
        // Gespeichert in UTC, angezeigt in lokaler Zeit des Nutzers.
        return entry.timestamp.toLocalTime().toString(QStringLiteral("HH:mm:ss"));
    case SenderNameRole:
        return entry.senderName;
    case MessageTextRole:
        return entry.text;
    case ChannelRole:
        switch (entry.channel) {
        case Channel::Broadcast: return QStringLiteral("Broadcast");
        case Channel::Direct:    return QStringLiteral("Direkt");
        case Channel::System:    return QStringLiteral("System");
        }
        return {};
    case OutgoingRole:
        return entry.outgoing;
    default:
        return {};
    }
}

QHash<int, QByteArray> ChatLogModel::roleNames() const
{
    // "messageText" statt "text": Ein Label in QML hat schon eine Eigenschaft
    // namens text – derselbe Name würde im Delegate kollidieren.
    return {
        {TimeRole, "time"},
        {SenderNameRole, "senderName"},
        {MessageTextRole, "messageText"},
        {ChannelRole, "channel"},
        {OutgoingRole, "outgoing"},
    };
}

void ChatLogModel::append(const Entry& entry)
{
    const int row = static_cast<int>(m_entries.size());
    // begin/end-Paar ist Pflicht: Es sagt der View, was sich geändert hat.
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    endInsertRows();
}

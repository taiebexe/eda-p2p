#include "core/Message.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>

namespace {
// Feldnamen an einer Stelle, damit toJson() und fromJson() garantiert
// dieselben Schlüssel benutzen.
const QString kVersion = QStringLiteral("v");
const QString kType = QStringLiteral("type");
const QString kSenderId = QStringLiteral("senderId");
const QString kSenderName = QStringLiteral("senderName");
const QString kTimestamp = QStringLiteral("timestamp");
const QString kPayload = QStringLiteral("payload");
const QString kText = QStringLiteral("text");
} // namespace

QByteArray Message::toJson() const
{
    const QJsonObject object{
        {kVersion, ProtocolVersion},
        {kType, type},
        // Ohne geschweifte Klammern: kürzer und in anderen Sprachen leichter zu lesen.
        {kSenderId, senderId.toString(QUuid::WithoutBraces)},
        {kSenderName, senderName},
        // ISO 8601 in UTC mit Millisekunden, z. B. "2026-11-10T09:15:02.123Z":
        // menschenlesbar, ohne Zeitzonen-Verwirrung und als Text sortierbar.
        {kTimestamp, timestamp.toUTC().toString(Qt::ISODateWithMs)},
        {kPayload, payload},
    };
    // Compact: keine Leerzeichen/Zeilenumbrüche -> kleinere UDP-Pakete.
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<Message> Message::fromJson(const QByteArray& data)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return std::nullopt;

    const QJsonObject object = document.object();

    // Jede Prüfung schützt vor fremden oder kaputten Paketen: Jeder im LAN
    // kann an unseren Port senden, auch Programme, die uns nicht kennen.
    if (object.value(kVersion).toInt(-1) != ProtocolVersion)
        return std::nullopt;

    Message message;
    message.type = object.value(kType).toString();
    if (message.type.isEmpty())
        return std::nullopt;

    message.senderId = QUuid::fromString(object.value(kSenderId).toString());
    if (message.senderId.isNull())
        return std::nullopt;

    message.senderName = object.value(kSenderName).toString();

    message.timestamp = QDateTime::fromString(object.value(kTimestamp).toString(), Qt::ISODateWithMs);
    if (!message.timestamp.isValid())
        return std::nullopt;

    const QJsonValue payload = object.value(kPayload);
    if (!payload.isObject())
        return std::nullopt;
    message.payload = payload.toObject();

    return message;
}

Message Message::makeText(const QUuid& senderId, const QString& senderName, const QString& text)
{
    Message message;
    message.type = MessageType::Text;
    message.senderId = senderId;
    message.senderName = senderName;
    message.timestamp = QDateTime::currentDateTimeUtc();
    message.payload = QJsonObject{{kText, text}};
    return message;
}

Message Message::makeHello(const QUuid& senderId, const QString& senderName)
{
    Message message;
    message.type = MessageType::Hello;
    message.senderId = senderId;
    message.senderName = senderName;
    message.timestamp = QDateTime::currentDateTimeUtc();
    // Leeres Objekt statt fehlendem Feld: payload ist laut Format immer ein Objekt.
    message.payload = QJsonObject{};
    return message;
}

QString Message::text() const
{
    return payload.value(kText).toString();
}

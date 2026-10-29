#pragma once

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <optional>

// One compact JSON object per UDP datagram or newline-delimited TCP frame.
namespace Message
{
constexpr int MaxBytes = 8192;
constexpr int MaxTextLength = 1000;

inline QByteArray encode(const QString& type, const QString& sender, const QJsonObject& payload)
{
    return QJsonDocument(
               QJsonObject{
                   {"type", type},
                   {"sender", sender},
                   {"timestamp", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
                   {"payload", payload},
               })
        .toJson(QJsonDocument::Compact);
}

inline std::optional<QJsonObject> decode(const QByteArray& data)
{
    if (data.size() > MaxBytes)
        return std::nullopt;
    const auto document = QJsonDocument::fromJson(data);
    const auto object = document.object();
    if (!document.isObject() || !object["type"].isString() || !object["sender"].isString() ||
        object["sender"].toString().isEmpty() || !object["timestamp"].isString() ||
        !object["payload"].isObject() ||
        !QDateTime::fromString(object["timestamp"].toString(), Qt::ISODateWithMs).isValid())
        return std::nullopt;
    return object;
}

inline bool validText(const QString& text)
{
    return !text.trimmed().isEmpty() && text.size() <= MaxTextLength;
}
} // namespace Message

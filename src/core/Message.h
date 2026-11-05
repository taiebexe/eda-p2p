#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QUuid>

#include <optional>

// Nachrichtentypen als Konstanten statt verstreuter String-Literale:
// Ein Tippfehler wird so zum Compilerfehler statt zum stillen Protokollfehler.
namespace MessageType {
inline const QString Text = QStringLiteral("text");    // Chattext (UDP und TCP)
inline const QString Hello = QStringLiteral("hello");  // erste Nachricht nach TCP-Verbindungsaufbau
// Später z. B.: Profile = "profile", Image = "image"
}

// Eine Nachricht unseres Protokolls – reine Daten, kein Netzwerk, kein QObject.
// Deshalb kann sie kopiert, in Listen gespeichert und ohne Sockets getestet werden.
//
// Auf der Leitung (UTF-8, kompaktes JSON):
// {"v":1,"type":"text","senderId":"…","senderName":"Anna",
//  "timestamp":"2026-11-10T09:15:02.123Z","payload":{"text":"Hallo"}}
struct Message
{
    // Protokollversion: Ändern wir das Format inkompatibel, erhöhen wir sie,
    // und alte Programme verwerfen die neuen Nachrichten sauber.
    static constexpr int ProtocolVersion = 1;

    QString type;
    QUuid senderId;       // eindeutige Peer-ID (pro Programmstart neu)
    QString senderName;   // Anzeigename, frei wählbar, nicht eindeutig
    QDateTime timestamp;  // immer UTC
    QJsonObject payload;  // typabhängiger Inhalt, z. B. {"text": "..."}

    // Serialisiert nach kompaktem JSON (UTF-8).
    QByteArray toJson() const;

    // Liest JSON ein. Liefert std::nullopt bei allem, was nicht exakt unserem
    // Format entspricht – Daten aus dem Netz sind nie vertrauenswürdig.
    static std::optional<Message> fromJson(const QByteArray& data);

    // Fabrikfunktionen für die Typen, die wir heute brauchen.
    static Message makeText(const QUuid& senderId, const QString& senderName, const QString& text);
    static Message makeHello(const QUuid& senderId, const QString& senderName);

    // Bequemer Zugriff auf den Text einer "text"-Nachricht.
    QString text() const;
};

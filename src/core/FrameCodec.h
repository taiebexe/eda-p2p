#pragma once

#include <QByteArray>

// TCP-Framing mit Längenpräfix.
//
// Problem: TCP ist ein Bytestrom. Zwei gesendete Nachrichten können beim
// Empfänger zusammengeklebt oder in Stücken ankommen. Der Empfänger braucht
// also eine Regel, wo eine Nachricht endet.
//
// Lösung: Vor jede Nachricht schreiben wir ihre Länge als 4-Byte-Zahl
// (quint32, Big Endian = "Netzwerk-Byte-Reihenfolge"):
//
//   [ 00 00 00 2A ][ 42 Bytes JSON ][ 00 00 00 10 ][ 16 Bytes JSON ] ...
//     Länge = 42      Nutzdaten        Länge = 16      Nutzdaten
//
// Die Klasse hat keine Socket-Abhängigkeit, damit sie ohne Netzwerk testbar ist.
class FrameCodec
{
public:
    static constexpr int HeaderSize = 4;
    // Obergrenze pro Nachricht. Schützt vor Müll-Daten: Ohne Grenze könnte
    // ein kaputter Header "4 GB folgen" behaupten und wir würden ewig puffern.
    // 16 MiB reicht später auch für Bilder.
    static constexpr quint32 MaxFrameSize = 16 * 1024 * 1024;

    // Packt Nutzdaten in einen Frame (Header + Daten). Größer als
    // MaxFrameSize ist ein Programmierfehler des Senders -> leeres Ergebnis.
    static QByteArray encode(const QByteArray& payload);

    enum class Result {
        Frame,         // ein vollständiger Frame wurde geliefert
        NeedMoreData,  // noch nicht genug Bytes da – auf nächstes readyRead warten
        Error,         // Header ungültig (zu groß) – Verbindung schließen
    };

    // Empfangene Bytes anhängen (beliebige Stückelung).
    void append(const QByteArray& bytes);

    // Versucht, den nächsten vollständigen Frame aus dem Puffer zu holen.
    // Muss in einer Schleife aufgerufen werden, bis nicht mehr Frame kommt,
    // denn ein einziges readyRead kann mehrere Nachrichten enthalten.
    Result next(QByteArray& frameOut);

    // Anzahl gepufferter, noch nicht verarbeiteter Bytes (für Tests).
    qsizetype bufferedBytes() const { return m_buffer.size(); }

private:
    QByteArray m_buffer;
};

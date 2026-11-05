# Architektur

## Grundidee

**Netzwerk-Klassen kennen die Oberfläche nicht, und die Oberfläche fasst nie einen Socket an.**
Sie reden nur über Signale und Slots. Dadurch kann man Netzwerk-Code ohne GUI testen
und später Funktionen ergänzen, ohne Bestehendes umzubauen.

```
┌──────────────────── qml/Main.qml ──────────────────────┐
│ Kopf: Name, IP, Ports · links: IP:Port + Verbinden,    │
│ Peers · rechts: Verlauf · unten: [Ziel ▾] [Text] Senden│
└──────┬───────────────────────────────▲─────────────────┘
       │ Q_INVOKABLE-Aufrufe            │ Properties, Modell
┌──────▼───────────────────────────────┴─────────────────┐
│ ChatController (src/app)  ── besitzt ── ChatLogModel    │
└──────┬──────────────────────────────┬──────────────────┘
       │ Signale/Slots                │ Signale/Slots
┌──────▼──────────┐           ┌───────▼─────────────────────┐
│ UdpService      │           │ TcpService                  │
│  QUdpSocket     │           │  QTcpServer (eingehend)     │
│  Broadcast      │           │  N × PeerConnection         │
└──────┬──────────┘           │       QTcpSocket+FrameCodec │
       │                      └───────┬─────────────────────┘
       └──────────── beide nutzen ────┴──► Message (src/core)
```

| Klasse | Ordner | Aufgabe |
|---|---|---|
| `Message` | core | Datenstruktur + JSON hin/zurück. Kein QObject, kein Netzwerk → einfach testbar. |
| `FrameCodec` | core | Längenpräfix-Framing für TCP. Ohne Socket → testbar mit zerstückelten Daten. |
| `UdpService` | network | UDP-Port binden, Broadcast senden, empfangen, eigene Nachrichten (UUID) verwerfen. |
| `PeerConnection` | network | Eine TCP-Verbindung: Socket + eigener Empfangspuffer, Timeout, Fehlerbehandlung. |
| `TcpService` | network | Server (eingehend) + Client (ausgehend), verwaltet Verbindungen über IDs. |
| `ChatLogModel` | app | Chatverlauf als `QAbstractListModel` für die QML-ListView. |
| `ChatController` | app | Brücke zu QML: Properties + `Q_INVOKABLE`-Methoden; verdrahtet Services. |
| `main.cpp` | src | Kommandozeile, Controller erzeugen, QML laden. |

### Bibliothek `p2pcore`
`core/` und `network/` werden zur statischen Bibliothek `p2pcore` gebaut. App **und** Tests linken
sie. Die Tests brauchen also kein QML, und nichts wird doppelt kompiliert.

## Zwei Instanzen auf einem PC

- **UDP:** Beide binden **denselben** Port (45454) mit `ShareAddress | ReuseAddressHint`.
  Der Port ist der gemeinsame „Kanal“. Ein Broadcast kommt auch beim Sender selbst an →
  Filter über `senderId` (QUuid). Die IP taugt nicht: beide Instanzen haben dieselbe.
- **TCP:** Zwei Programme können **nicht** auf demselben TCP-Port lauschen →
  jede Instanz bekommt ihren eigenen Port (`--tcp-port`). Ist er belegt, wählt die App
  automatisch einen freien (Port 0) und zeigt ihn an.

## Nachrichtenformat

Eine Nachricht = ein JSON-Objekt (UTF-8, kompakt):

```json
{
  "v": 1,
  "type": "text",
  "senderId": "6f1c2a9e-…",
  "senderName": "Anna",
  "timestamp": "2026-11-10T09:15:02.123Z",
  "payload": { "text": "Hallo zusammen" }
}
```

| Feld | Entscheidung |
|---|---|
| `v` | Protokollversion. Andere Versionen werden verworfen statt falsch gelesen. |
| `type` | String, lesbar in Wireshark/Logs. Heute `text` und `hello` (Vorstellung nach TCP-Aufbau). |
| `senderId` | QUuid pro Programmstart – eindeutig, auch bei gleicher IP. |
| `senderName` | Anzeigename, frei wählbar, nicht eindeutig. |
| `timestamp` | ISO 8601 in **UTC** mit Millisekunden: lesbar, keine Zeitzonenprobleme, als Text sortierbar. |
| `payload` | **Immer ein Objekt**, damit neue Typen eigene Felder bekommen, ohne das Außenformat zu ändern. |

`Message::fromJson()` liefert `std::optional<Message>`: Fremde oder kaputte Pakete (jeder im LAN
kann an unseren Port senden) werden still verworfen statt das Programm abstürzen zu lassen.

## TCP-Framing

**UDP** behält Nachrichtengrenzen: ein `writeDatagram` = genau ein Datagramm beim Empfänger.

**TCP** ist ein **Bytestrom**. Sendet man A (50 Bytes) und B (80 Bytes), kann `readyRead` liefern:
130 Bytes auf einmal, oder 20 + 110 Bytes, oder jede andere Aufteilung.

Lösung: **Längenpräfix.** Vor jede Nachricht kommen 4 Bytes mit ihrer Länge (quint32, Big Endian):

```
[00 00 00 2A][42 Bytes JSON][00 00 00 10][16 Bytes JSON]…
```

| | Trennzeichen (`\n`) | **Längenpräfix (gewählt)** |
|---|---|---|
| Vorteil | im Debugger lesbar | funktioniert auch für Binärdaten (Bilder), kein Byte-für-Byte-Suchen |
| Nachteil | bricht bei Binärdaten | 4 Bytes Overhead |

Empfangsalgorithmus (`FrameCodec::next`, in einer Schleife aufgerufen):
1. Neue Bytes an den Puffer hängen.
2. Weniger als 4 Bytes? → warten.
3. Länge N lesen. N > 16 MiB → Müll/Angriff → Verbindung trennen.
4. Weniger als 4 + N Bytes? → warten (Header **nicht** entfernen).
5. Sonst Frame herausschneiden, aus dem Puffer löschen, weiter bei 2 –
   denn ein `readyRead` kann mehrere Nachrichten enthalten.

## Ablauf einer TCP-Verbindung

```
Ben (Client)                                   Anna (Server)
connectToPeer("192.168.0.10:50001")
  QTcpSocket::connectToHost  ── SYN ──►        QTcpServer::newConnection
  connected → peerConnected                    peerConnected
  send(hello{Ben})            ────────►        hello → Name "Ben" in Peer-Liste
                              ◄────────        send(hello{Anna})
hello → Name "Anna"
sendDirect("Hi")              ────────►        messageReceived → Chatverlauf
```

Fehlerfälle: Ziel lehnt ab → `ConnectionRefusedError` → Meldung im Chatverlauf.
Ziel antwortet nicht → nach 5 s eigener Timeout (`QTimer`). Gegenüber schließt → normale
Trennungsmeldung, kein Fehler. Verbindung zu sich selbst → per `hello.senderId` erkannt und getrennt.

## Erweiterbarkeit (bewusst noch nicht gebaut)

| Feature | Wo es hingehört |
|---|---|
| Öffentliches Profil | Neuer Typ `profile`, alle paar Sekunden per UDP-Broadcast → liefert nebenbei eine Liste der Online-Peers. |
| Bilder | Neuer Typ `image` über **TCP** (Längenpräfix kann beliebige Größen bis 16 MiB). Per UDP ungeeignet: max. ~64 KB, Fragmentierung. |
| Verlauf | Neue Klasse `HistoryStore`, verbunden mit den vorhandenen Signalen; speichert unter `QStandardPaths::AppDataLocation`. Kann `ChatLogModel` beim Start füllen. |

Keine der Erweiterungen erfordert Änderungen an `UdpService`, `TcpService` oder am Außenformat.

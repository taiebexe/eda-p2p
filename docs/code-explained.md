# Der Code erklärt

Lesereihenfolge: von innen nach außen – erst die Daten (`core`), dann das Netzwerk,
dann die Brücke zu QML, zuletzt Start und Oberfläche.

---

## 0. C++/Qt-Grundlagen für Java- und Python-Leute

| Begriff | Java/Python-Vergleich | Im Projekt |
|---|---|---|
| `.h` + `.cpp` | Interface + Implementierung. Der Header sagt *was* es gibt, die `.cpp` *wie*. Andere Dateien inkludieren nur den Header. | jede Klasse |
| `#pragma once` | verhindert doppeltes Einbinden desselben Headers | jeder Header |
| `const QString&` | „übergib per Referenz, ändere nichts“ – spart Kopien, wie eine Java-Referenz, die man nicht verändern darf | fast alle Parameter |
| `std::optional<T>` | wie Javas `Optional<T>` / Pythons `T | None` | `Message::fromJson` |
| Zeiger `T*` | Referenz, die auch `nullptr` sein kann. Wer ihn löscht, muss geregelt sein. | Sockets, Services |
| **Parent-Child** | Qt-Speicherverwaltung: `new QUdpSocket(this)` → das Kind wird automatisch gelöscht, wenn der Parent stirbt. Deshalb steht nirgends `delete`. | alle QObjects |
| `Q_OBJECT` + moc | Makro, für das Qts *Meta-Object-Compiler* zusätzlichen Code erzeugt (Signale, Properties, Reflexion). CMake ruft moc automatisch auf (AUTOMOC). | jede QObject-Klasse |
| **Signal/Slot** | wie Listener/Observer: `connect(sender, &Sender::signal, receiver, &Receiver::slot)`. Der Sender kennt den Empfänger nicht. | überall |
| Lambda `[this, id](...) { ... }` | wie Java-Lambda / Python-`lambda`. In `[...]` steht, was „eingefangen“ wird. | TcpService, PeerConnection |
| Ereignisschleife | `app.exec()` läuft, bis das Fenster zu ist, und verteilt Ereignisse (Mausklick, „Daten am Socket“) an Slots. **Nie blockieren** (`waitFor…`, `sleep`), sonst friert die GUI ein. | main.cpp |

---

## 1. `src/core/Message.h` / `Message.cpp`

Datenstruktur einer Nachricht plus Umwandlung in JSON und zurück.

- `struct` statt `class`: nur öffentliche Daten, keine Logik zu verstecken.
- `toJson()` baut ein `QJsonObject` und serialisiert es **kompakt**.
- `fromJson()` prüft Feld für Feld (Version, Typ, gültige UUID, gültiger Zeitstempel,
  payload ist Objekt) und gibt bei jedem Fehler `std::nullopt` zurück.
- `makeText()`/`makeHello()` sind Fabrikfunktionen: Sie setzen Typ und Zeitstempel richtig,
  damit das niemand vergisst.
- `MessageType::Text` als `inline const QString`: Tippfehler in Typnamen werden Compilerfehler.

**Was du hier verstehen musst**
- Daten aus dem Netz sind **nicht vertrauenswürdig**. Deshalb liefert `fromJson` ein `optional`
  und prüft alles. Kein `throw`, kein Absturz bei Müll.
- Die Klasse kennt weder Sockets noch QML. Das macht sie im Unit-Test ohne Netzwerk prüfbar.
- Zeitstempel werden in **UTC** übertragen und erst in der Anzeige in Ortszeit umgerechnet.
- `payload` ist immer ein Objekt. Neue Nachrichtentypen (Profil, Bild) brauchen kein neues Außenformat.
- `v` (Version) erlaubt später inkompatible Änderungen, ohne dass alte Programme Unsinn anzeigen.

---

## 2. `src/core/FrameCodec.h` / `FrameCodec.cpp`

Trennt den TCP-Bytestrom wieder in einzelne Nachrichten (Längenpräfix-Framing).

- `encode()`: 4-Byte-Header (Länge, Big Endian via `qToBigEndian`) + Nutzdaten.
- `append()`: hängt empfangene Bytes an einen internen Puffer.
- `next()`: liefert `Frame`, `NeedMoreData` oder `Error`. Der Header wird erst entfernt, wenn
  die ganze Nachricht da ist.
- `MaxFrameSize` (16 MiB) schützt vor kaputten Headern, die „4 GB folgen“ behaupten.

**Was du hier verstehen musst**
- TCP garantiert Reihenfolge und Vollständigkeit, aber **keine Nachrichtengrenzen**.
- Big Endian ist die „Netzwerk-Byte-Reihenfolge“. Ohne Umwandlung würden Rechner mit
  verschiedener CPU die Länge unterschiedlich lesen.
- `next()` muss **in einer Schleife** aufgerufen werden: Ein Paket kann mehrere Nachrichten enthalten.
- Die Klasse hat absichtlich keinen Socket. Der Test simuliert Stückelung und Verkleben
  (`tests/tst_framecodec.cpp`).

---

## 3. `src/network/UdpService.h` / `UdpService.cpp`

Broadcast-Chat: senden an alle, empfangen von allen.

- Konstruktor: legt `QUdpSocket` als Kind an, verbindet `readyRead` mit `onReadyRead`.
- `start(port)`: `bind(AnyIPv4, port, ShareAddress | ReuseAddressHint)`.
- `broadcastAddresses()`: fragt `QNetworkInterface` nach allen aktiven Netzwerkkarten und sammelt
  deren Broadcast-Adresse (z. B. `10.1.255.255` für das Netz `10.1.x.x/16`).
- `sendBroadcast()`: `writeDatagram(data, adresse, port)` **für jede dieser Adressen**. Erfolg, wenn
  mindestens eine Karte klappt. Nur wenn gar keine Karte gefunden wird: `255.255.255.255`.
- `onReadyRead()`: `while (hasPendingDatagrams())` → `receiveDatagram()` → Duplikat? verwerfen →
  `fromJson` → eigene ID verwerfen → Signal `messageReceived`.

**Was du hier verstehen musst**
- **ShareAddress | ReuseAddressHint** erlaubt mehreren Programmen denselben UDP-Port – nur so laufen
  zwei Instanzen auf einem PC, und beide erhalten jeden Broadcast.
- Der **eigene Broadcast kommt zurück**. Die IP hilft nicht (gleicher PC = gleiche IP), deshalb die QUuid.
- **while statt if** in `onReadyRead`: `readyRead` kommt einmal, auch wenn mehrere Datagramme warten.
- UDP braucht kein Framing (ein Datagramm = eine Nachricht), aber es gibt **keine Zustellgarantie**.
- `AnyIPv4`, weil es Broadcast nur bei IPv4 gibt. Die Größe ist auf 8 KB begrenzt, um IP-Fragmentierung zu vermeiden.
- **Warum nicht einfach 255.255.255.255?** Diese Adresse gehört zu keinem Netz; das Betriebssystem muss
  raten, über welche Karte sie geht. Auf unserem Mac kam dabei zeitweise *„No route to host“*
  („Unable to send a message“) – mal sekundenlang, mal über eine Minute. Die Broadcast-Adresse des
  konkreten Netzes (`10.1.255.255`) klappte in allen Versuchen. Windows schickt 255.255.255.255 außerdem
  nur über **eine** Karte, evtl. die falsche (VPN, VirtualBox).
- **Duplikate:** Wer zwei Netzwerkkarten hat, bekommt denselben Broadcast zweimal. Die Bytes sind dann
  identisch (gleiche `senderId`, gleicher Zeitstempel auf die Millisekunde), also merken wir uns die
  letzten 32 Datagramme und verwerfen Wiederholungen.

---

## 4. `src/network/PeerConnection.h` / `PeerConnection.cpp`

Eine einzelne TCP-Verbindung, egal ob ein- oder ausgehend.

- Übernimmt den `QTcpSocket` (`setParent(this)`) und verbindet `connected`, `readyRead`,
  `errorOccurred`, `disconnected`.
- `connectToHost()` startet den Aufbau und einen 5-s-`QTimer::singleShot` als Timeout.
- `send()`: `FrameCodec::encode(message.toJson())` → `write()`.
- `onReadyRead()`: `readAll()` → Codec → Schleife über `next()` → Signal pro Nachricht.
- `onSocketError()`: `RemoteHostClosedError` ist normales Ende, keine Fehlermeldung.
- `markClosed()`: sorgt dafür, dass `closed()` **genau einmal** kommt.

**Was du hier verstehen musst**
- Jede Verbindung braucht **ihren eigenen Puffer**. Deshalb eine Klasse pro Verbindung.
- `connectToHost()` und `write()` **blockieren nicht**. Ergebnisse kommen als Signale. Das ersetzt
  `waitForConnected()` und hält die GUI flüssig.
- Ein gescheiterter Verbindungsaufbau sendet **kein** `disconnected()` (es gab ja nie eine Verbindung).
  Deshalb wird der Zustand im Fehler-Slot geprüft.
- Das Lambda im Timer hat `this` als Kontextobjekt: Wird die Verbindung vorher gelöscht, läuft es nicht mehr.

---

## 5. `src/network/TcpService.h` / `TcpService.cpp`

Server und Client in einem – das „Peer“ in Peer-to-Peer.

- `listen(port)`: `QTcpServer::listen(QHostAddress::Any, port)`. Port 0 = Betriebssystem wählt.
- `onNewConnection()`: `while (hasPendingConnections())` → `nextPendingConnection()` → `PeerConnection`.
- `connectToPeer()`: neuer `QTcpSocket` → `PeerConnection` → `connectToHost()`.
- `addConnection()`: vergibt eine `int`-ID, reicht die Signale der Verbindung mit ID weiter.
- Bei `closed`: aus der Tabelle entfernen, `peerDisconnected` senden, `deleteLater()`.

**Was du hier verstehen musst**
- Jede Instanz ist **gleichzeitig Server und Client**. Es gibt keine feste Server-Rolle.
- Nach außen nur **IDs statt Zeiger**: Niemand kann ein schon gelöschtes Objekt benutzen.
- **`deleteLater()` statt `delete`**: Wir sind gerade in einem Signal genau dieses Objekts.
  Gelöscht wird, sobald die Ereignisschleife wieder dran ist.
- Lambdas „reichern“ Signale an (`[this, id]`), sodass der Controller `PeerConnection` gar nicht kennen muss.

---

## 6. `src/app/ChatLogModel.h` / `ChatLogModel.cpp`

Der Chatverlauf als Listenmodell für QML.

- Erbt von `QAbstractListModel`. Pflicht: `rowCount`, `data`, `roleNames`.
- Rollen (`time`, `senderName`, `messageText`, `channel`, `outgoing`) erscheinen in QML als Eigenschaften im Delegate.
- `append()` umschließt das Einfügen mit `beginInsertRows`/`endInsertRows`.
- `QML_ELEMENT` + `QML_UNCREATABLE`: QML kennt den Typ, darf ihn aber nicht selbst erzeugen.

**Was du hier verstehen musst**
- Model/View: Das Modell hält Daten, die View (ListView) zeichnet sie. Das Modell **meldet Änderungen**,
  statt dass die View pollt.
- Ohne `beginInsertRows`/`endInsertRows` bemerkt die ListView neue Zeilen nicht (oder stürzt ab).
- Rolle heißt `messageText` statt `text`, weil `Label` schon eine Eigenschaft `text` hat.
- Hier kann später der Verlauf (`HistoryStore`) beim Start alte Nachrichten einfüllen.

---

## 7. `src/app/ChatController.h` / `ChatController.cpp`

Die Brücke zwischen QML und Netzwerk.

- Erzeugt die eigene `QUuid`, besitzt `UdpService`, `TcpService`, `ChatLogModel`.
- `Q_PROPERTY`s: `displayName`, `udpPort`, `tcpPort`, `localAddresses`, `peers`, `chatLog`.
- `Q_INVOKABLE`: `sendBroadcast`, `connectToPeer`, `sendDirect` – geben `bool` zurück.
- `start()`: Ports öffnen. Ist der TCP-Port belegt → Port 0 (automatisch).
- Bei TCP-Verbindung: sofort `hello` senden. Bei empfangenem `hello`: Namen in Peer-Liste eintragen;
  ist es die eigene ID → Verbindung trennen.
- Alle Fehler landen als „System“-Zeile im Chatverlauf.

**Was du hier verstehen musst**
- QML sieht nur **Properties** (mit `NOTIFY`-Signal → automatische Aktualisierung) und **`Q_INVOKABLE`**-Methoden.
- Der Controller übersetzt zwischen Nutzer-Aktionen und Protokoll. Die Services bleiben reine Transportschicht.
- `connectToPeer` kehrt **sofort** zurück. „Verbunden“ oder „Fehler“ erscheinen später über Signale.
- Eigene Nachrichten trägt der Controller selbst in den Verlauf ein (das UDP-Echo wird ja gefiltert).
- Der `hello`-Austausch ist ein Mini-Handshake. Später kann er Profil-Infos mitbringen.

---

## 8. `src/main.cpp`

Startpunkt.

- `QCommandLineParser` liest `--name`, `--udp-port`, `--tcp-port` (mit `--help`).
- Erzeugt `ChatController`, ruft `start()`.
- `QQmlApplicationEngine` + `setInitialProperties({{"controller", …}})` + `loadFromModule("EdaP2P", "Main")`.
- `objectCreationFailed` → Programm mit Fehlercode beenden.

**Was du hier verstehen musst**
- **Reihenfolge der Variablen = umgekehrte Zerstörungsreihenfolge**. Controller vor Engine → Engine
  stirbt zuerst, QML greift nie auf einen gelöschten Controller zu.
- `setInitialProperties` + `required property` in QML ist der Qt-6-Weg. Das ältere
  `setContextProperty` ist ein globaler, untypisierter Name, den qmllint nicht prüfen kann.
- `app.exec()` startet die Ereignisschleife. Ab hier passiert alles über Signale.

---

## 9. `qml/Main.qml`

Die Oberfläche – deklarativ, ohne Logik.

- `required property ChatController controller` – wird aus C++ gesetzt.
- `import QtQuick.Controls.Material`: fester **Material-Stil**, damit die App auf Windows, macOS und Linux
  gleich aussieht. In `main.cpp` wird die kompakte Desktop-Variante `Dense` gewählt.
- Aufbau: Kopfzeile (Name, Meine IP, TCP-/UDP-Port) · links *Direktverbindung (TCP)* mit IP:Port,
  *Verbinden* und Liste der Verbundenen · rechts der Verlauf · unten **ein** Eingabefeld.
- Das Ziel wählt die Auswahlliste unten: Eintrag 0 = *An alle (UDP)*, danach je ein Eintrag pro Peer.
  `targetIndex` merkt sich die Wahl; `send()` ruft je nach Ziel `sendBroadcast` oder `sendDirect`.
- Verlauf als **Sprechblasen**: eigene rechts, fremde links, grün = an alle, orange = direkt,
  Systemmeldungen klein und grau in der Mitte.
- `textFormat: Text.PlainText`: Text aus dem Netz wird **nie als HTML** interpretiert.

**Was du hier verstehen musst**
- **Bindungen**: `text: controller.tcpPort` aktualisiert sich von selbst, sobald das `NOTIFY`-Signal kommt.
- `enabled: window.targetIndex >= 0 && messageInput.text.trim().length > 0` ist ebenfalls eine Bindung –
  der Senden-Button schaltet sich von selbst ein und aus.
- Fällt ein Peer weg, verschieben sich die Listenplätze. Damit eine private Nachricht nie beim
  falschen Empfänger oder bei allen landet, setzt die Oberfläche das Ziel dann auf „Empfänger wählen …“.
- Die Breite einer Sprechblase misst `TextMetrics` (Textbreite ohne Umbruch). `implicitWidth` des Labels
  wäre nach dem Umbruch schon die umgebrochene Breite → Blase zu schmal.
- QML-Datei = was man sieht. C++ = was passiert. Netzwerkcode in QML wäre schwer zu testen.
- `PlainText` ist eine Sicherheitsentscheidung: Sonst könnte jemand im LAN HTML/Links einschleusen.

---

## 10. `CMakeLists.txt` und `CMakePresets.json`

- `p2pcore` (statische Bibliothek) ← `eda-p2p` (App) und `tst_*` (Tests).
- `qt_standard_project_setup(REQUIRES 6.5)` schaltet AUTOMOC ein und setzt Qt-6-Standards.
- `qt_add_qml_module` packt `Main.qml` + C++-Typen mit `QML_ELEMENT` in das Modul `EdaP2P`.
- Presets: eine gemeinsame Konfiguration für Visual Studio, Qt Creator und die Kommandozeile.
  Der Qt-Pfad kommt aus `QTDIR`, damit keine persönlichen Pfade im Repo landen.

**Warum CMake statt nur `.vcxproj`?**
- `.vcxproj` gibt es nur in Visual Studio unter Windows. Die App muss aber auf macOS und Linux bauen.
- CMake ist **Qts offizielles Build-System** seit Qt 6 (qmake ist nur noch Legacy).
- Eine Quelle der Wahrheit: Jedes Teammitglied nutzt die IDE seiner Wahl (VS, Qt Creator, CLion, VS Code).
- Textdatei, gut in Git zu diffen und zu mergen. `.vcxproj` ist XML voller GUIDs und erzeugt Merge-Konflikte.
- Tests (`ctest`) und später CI (GitHub Actions) laufen ohne IDE.

---

## 11. Tests

| Datei | Prüft |
|---|---|
| `tst_message.cpp` | Rundreise JSON (inkl. Umlaute/Emoji), UTC-Format, 10 Arten ungültiger Eingaben (datengetrieben). |
| `tst_framecodec.cpp` | Header-Format, Byte-für-Byte-Empfang, mehrere Frames in einem Stück, zu großer Header. |
| `tst_network.cpp` | Echte Sockets: TCP in beide Richtungen, 5 schnelle Nachrichten, Trennen ohne Fehler, abgelehnte Verbindung, UDP-Broadcast an andere aber nicht an sich selbst, doppelt ankommender Broadcast wird nur einmal gemeldet. |

Die UDP-Tests **überspringen** sich (SKIP statt FAIL), wenn das System keinen Broadcast erlaubt –
etwa ohne Netzwerk oder wenn eine Firewall ihn blockiert.

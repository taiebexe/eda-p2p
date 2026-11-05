# Präsentation Meilenstein 10.11.2026

## 1. Entwicklungsumgebung

- Alle drei haben **Qt 6** + **CMake** + Compiler installiert. Mindestens **Qt 6.5** ist nötig
  (so steht es in `CMakeLists.txt`: `find_package(Qt6 6.5 ...)`).
  - Taieb Bourbia: macOS, **Qt 6.12.0**, CMake 3.30.5, Ninja, Apple Clang 21 – baut, alle 3 Tests grün
  - Die beiden Windows-Rechner im Team: Visual Studio 2022 + Qt VS Tools, MSVC 2022 (64-bit),
    Qt 6 über den Qt Online Installer. Projekt wird per „Ordner öffnen“ mit `CMakePresets.json` geladen.
- Ein gemeinsames Build-System: **CMake + CMakePresets.json**. Jede/r nutzt die IDE der Wahl.
- Beweis: Projekt baut auf **Windows und macOS**, `ctest` → alle Tests grün.

## 2. Team-Workflow

- GitHub-Repo, `main` geschützt, Arbeit nur über **Feature-Branches + Pull Requests**
- Jeder PR braucht **1 Review** von einem anderen Teammitglied
- Aufgaben als **GitHub Issues**, Fortschritt im **Project Board**
- Aufteilung: Protokoll + Tests · Netzwerk · Oberfläche + Doku. Jede/r reviewt die anderen Bereiche.
- Konventionen: Bezeichner Englisch, Kommentare/UI Deutsch. Details in `CONTRIBUTING.md`.

## 3. Architektur (1 Folie)

- Diagramm aus `docs/architecture.md`
- Drei Schichten: **QML** (Anzeige) → **ChatController** (Brücke) → **UdpService / TcpService** (Netzwerk)
- Nachricht = **JSON** mit `type, senderId, senderName, timestamp, payload`
- TCP-Framing mit **4-Byte-Längenpräfix**
- Alles über **Signale/Slots**, kein Blockieren, keine eigenen Threads

## 4. Demo-Ablauf (ca. 5 Minuten)

Vorbereitung: beide Laptops im **Handy-Hotspot**, App vorher einmal gestartet
(Firewall-/macOS-Dialog bestätigt), Schriftgröße groß, Benachrichtigungen aus.

1. **Laptop 1** zeigen: `--name Anna`. Oben „Meine IP“ und Ports erklären.
2. **Laptop 2** starten: `--name Ben`.
3. **UDP-Broadcast**: Anna schreibt „Hallo an alle“ → erscheint bei Ben.
   Hinweis: Anna sieht die Nachricht nur einmal → eigener Broadcast wird über die UUID gefiltert.
4. Ben antwortet per Broadcast.
5. **TCP**: Ben gibt links Annas IP (oben in Annas Fenster bei *Meine IP*) mit `:45455` ein → *Verbinden*
   → beide sehen „… ist jetzt direkt verbunden“ und den Namen (hello-Handshake).
6. **Direktnachrichten** in beide Richtungen: links auf den Namen klicken, schreiben, *Senden*.
   Farben erklären: **grün** = an alle (UDP), **orange** = direkt (TCP).
7. **Fehlerfall**: Anna schließt die App → Ben bekommt „Verbindung getrennt“, App läuft weiter.
8. Optional: **zwei Instanzen auf einem Laptop** (`--tcp-port 50001/50002`) – zeigt `ShareAddress`.
9. Optional: `ctest` live – 3 Testprogramme grün.

**Plan B** (Netz geht nicht): Demo mit zwei Instanzen auf einem Laptop (Testplan Abschnitt 1).

## 5. Wie wir KI genutzt haben

- KI-Assistent (Claude) als **Tutor und Pair-Programmer**: Architekturvorschlag, Code-Gerüst,
  Erklärungen, Testplan
- Vorgehen in Phasen: Plan → Build-Setup → Code → Tests → Präsentation. Nach jeder Phase haben wir
  geprüft und Entscheidungen selbst getroffen (z. B. **QML statt Widgets**, Deutsch als Teamsprache)
- **Alles selbst gebaut, getestet und Zeile für Zeile durchgesprochen**. Die Erklärungen stehen in
  `docs/code-explained.md`
- Grenzen: KI kann Netzwerkprobleme vor Ort (Firewall, Uni-WLAN) nicht sehen. Die haben wir selbst
  getestet und gelöst
- Unsere Erfahrung:
  - **Gut:** Erklärungen zu C++-Eigenheiten (Zeiger, `deleteLater`, `std::optional`) und zu TCP-Framing
    haben uns schneller weitergebracht als reines Suchen in Foren.
  - **Korrigiert:** Der erste Vorschlag nutzte Qt Widgets. Weil der Kurs QML nutzt, haben wir auf QML
    umgestellt.
  - **Korrigiert:** Der Compiler-Schalter `-Wpedantic` erzeugte nur Warnungen aus Qts eigenen Makros und
    wurde entfernt.
  - **Korrigiert:** Beim ersten Test auf zwei Fenstern kam bei jedem Broadcast „Unable to send a message“.
    Die KI hatte das zuerst auf die macOS-Berechtigung „Lokales Netzwerk“ geschoben – das war falsch.
    Ein kleines Testprogramm zeigte die echte Ursache: macOS lehnt `255.255.255.255` zeitweise ab
    („No route to host“), die Netz-Broadcast-Adresse (`10.1.255.255`) klappt immer. Jetzt senden wir an
    die Broadcast-Adresse jeder Netzwerkkarte.
  - **Gelernt:** Ein grüner Test heißt nicht automatisch, dass er gelaufen ist (SKIP lesen!), und eine
    plausible Erklärung ist erst richtig, wenn man sie gemessen hat.

## 6. Offene Fragen an den Betreuer

- Dürfen wir für die Abgabe/Demo einen Handy-Hotspot nutzen, oder muss es im Uni-Netz laufen?
- Soll die Peer-Erkennung automatisch per Broadcast erfolgen (Profil), oder reicht IP:Port manuell?
- Wie soll der lokale Verlauf gespeichert werden – JSON-Datei, SQLite (Qt SQL) oder egal?
- Gibt es Vorgaben zur Bildgröße beim Bild-Broadcast?
- Doppelte Verbindung, wenn A und B sich gleichzeitig verbinden – reicht das für jetzt?

## 7. Nächste Schritte

1. **Profil**: Typ `profile` per UDP alle 5 s → automatische Liste der Online-Peers,
   Verbinden per Klick statt IP eintippen
2. **Bilder**: Typ `image` über TCP (Längenpräfix ist schon binärfähig)
3. **Verlauf**: `HistoryStore` speichert Nachrichten lokal (`QStandardPaths::AppDataLocation`)
4. GitHub Actions: automatisch bauen + testen auf Windows, macOS, Linux

---

## 8. Mögliche Betreuerfragen

**1. Woher weiß der Empfänger bei TCP, wo eine Nachricht endet?**
TCP ist ein Bytestrom ohne Nachrichtengrenzen. Wir schreiben vor jede Nachricht ihre Länge als 4-Byte-Zahl
(Big Endian). Der Empfänger puffert, bis Header und N Bytes da sind, und verarbeitet in einer Schleife,
weil ein Paket mehrere Nachrichten enthalten kann. Längen über 16 MiB gelten als ungültig → Verbindung trennen.
Getestet in `tst_framecodec` (Byte-für-Byte und zusammengeklebt).

**2. Warum empfangt ihr eure eigenen Broadcasts nicht doppelt, und warum nutzt ihr dafür nicht die IP?**
Ein Broadcast geht an alle, auch an den Sender. Zwei Instanzen auf einem PC haben dieselbe IP. Deshalb bekommt
jede Instanz beim Start eine zufällige `QUuid`, die in jeder Nachricht als `senderId` steht. Ist sie gleich
der eigenen, wird die Nachricht verworfen.

**3. Wie können zwei Instanzen auf einem PC denselben UDP-Port nutzen, aber nicht denselben TCP-Port?**
Beim UDP-`bind` setzen wir `ShareAddress | ReuseAddressHint` (SO_REUSEADDR). Dann dürfen mehrere Sockets
denselben Port binden, und Broadcasts gehen an alle. Ein TCP-Server-Port muss eindeutig sein, damit das
Betriebssystem neue Verbindungen einem Programm zuordnen kann. Deshalb `--tcp-port` bzw. automatischer
Ausweich-Port.

**4. Warum friert die Oberfläche nicht ein, obwohl ihr keine Threads benutzt?**
Alle Qt-Netzwerkaufrufe sind asynchron: `connectToHost` und `write` kehren sofort zurück, Ergebnisse kommen
als Signale (`connected`, `readyRead`, `errorOccurred`) über die Ereignisschleife. Wir benutzen kein
`waitFor…`. Selbst der Verbindungs-Timeout ist ein `QTimer`, kein Warten.

**5. Was passiert, wenn jemand Müll an euren Port schickt oder die Verbindung abbricht?**
`Message::fromJson` prüft Version, Typ, UUID, Zeitstempel und payload und liefert sonst `std::nullopt` →
Paket wird ignoriert. Ein kaputter TCP-Header (zu groß) trennt die Verbindung. Text wird in QML als `PlainText`
angezeigt (kein HTML-Einschleusen). Abbrüche melden `disconnected`/`errorOccurred` → Peer verschwindet aus
der Liste, `deleteLater` räumt auf.

**Bonus: Warum QML und nicht Qt Widgets?** / **Warum CMake?**
QML trennt Darstellung (deklarativ) klar von Logik (C++). Die C++-Klassen sind über `QML_ELEMENT`
typsicher eingebunden. CMake ist seit Qt 6 Qts Standard-Build-System und läuft auf allen drei
Betriebssystemen, in jeder IDE und in CI. Ein `.vcxproj` gibt es nur unter Windows/Visual Studio.

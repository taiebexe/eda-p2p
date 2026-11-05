# EDA P2P – LAN-Chat ohne Server

Plattformübergreifende Desktop-App (C++17, Qt 6, QML) für Peer-to-Peer-Kommunikation
im lokalen Netz. Projekt im Modul EDA, TH Köln, WS 2026/27.

**Stand Meilenstein 10.11.2026**
- Textnachrichten an alle im LAN per **UDP-Broadcast**
- **TCP-Direktverbindung** zwischen zwei Instanzen mit Direktnachrichten
- Zwei Instanzen auf einem PC möglich (konfigurierbare Ports)
- Unit-Tests (Qt Test) für Nachrichtenformat, Framing und Netzwerk

| Dokument | Inhalt |
|---|---|
| [docs/architecture.md](docs/architecture.md) | Klassen, Nachrichtenformat, TCP-Framing, Erweiterbarkeit |
| [docs/code-explained.md](docs/code-explained.md) | Jede Datei erklärt + „Was du hier verstehen musst“ |
| [docs/testplan.md](docs/testplan.md) | Testplan und Fehlersuche (Firewall, WLAN, Ports) |
| [docs/presentation.md](docs/presentation.md) | Stichpunkte für die Präsentation + Betreuerfragen |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Git-Workflow im Team |

## Voraussetzungen

- **Qt 6.5 oder neuer** (getestet mit 6.12.0), Komponenten: *Qt Quick*, *Qt Network*
  (beide im Standard-Desktop-Paket enthalten)
- **CMake ≥ 3.21** und **Ninja** (beides über den Qt-Installer unter *Developer and Designer Tools* installierbar)
- Compiler: Windows **MSVC 2022** (Visual Studio 2022) · macOS **Xcode Command Line Tools** · Linux **GCC/Clang**

### Einmalig: Umgebungsvariable `QTDIR`

`CMakePresets.json` findet Qt über `QTDIR`. So steht kein persönlicher Pfad im Repo.

| System | Beispielwert |
|---|---|
| Windows | `C:\Qt\6.12.0\msvc2022_64` |
| macOS | `$HOME/Qt/6.12.0/macos` |
| Linux | `$HOME/Qt/6.12.0/gcc_64` |

Windows: *Systemsteuerung → System → Erweiterte Systemeinstellungen → Umgebungsvariablen*,
neue Benutzervariable `QTDIR`. Danach Visual Studio **neu starten**.
macOS/Linux: `export QTDIR=...` in `~/.zshrc` bzw. `~/.bashrc`.

## Bauen und Starten

### Windows – Visual Studio 2022

1. Visual Studio Installer: Workload **„Desktopentwicklung mit C++“** (enthält CMake, Ninja und MSVC).
2. Optional: Erweiterung **Qt VS Tools** (Hilfe zu Qt-Typen, QML-Syntaxfarben). Für das
   Bauen ist sie *nicht* nötig – das macht CMake.
3. *Datei → Öffnen → Ordner…* → Repo-Ordner wählen. Visual Studio erkennt `CMakePresets.json`.
4. Oben in der Symbolleiste Konfiguration **„Debug (Windows, MSVC x64)“** wählen.
5. Startelement **eda-p2p.exe** wählen → **F5** (Debuggen) oder **Strg+F5**.
6. Parameter für die zweite Instanz: Rechtsklick auf `CMakeLists.txt` →
   *Debug- und Starteinstellungen hinzufügen* → in `launch.vs.json` ergänzen:
   ```json
   "args": [ "--name", "Ben", "--tcp-port", "50002" ]
   ```
   Oder einfach die exe aus `build\windows-debug\` per PowerShell starten (siehe unten).
7. Tests: *Test → Test-Explorer* oder `ctest --preset windows-debug` in der *Developer PowerShell*.

> Fehlt beim Starten außerhalb von Visual Studio eine `Qt6Core.dll`, liegt
> `%QTDIR%\bin` nicht im `PATH`. Entweder in den PATH aufnehmen oder einmal
> `%QTDIR%\bin\windeployqt.exe --qmldir qml build\windows-debug\eda-p2p.exe` ausführen.

Kommandozeile (Developer PowerShell for VS 2022):

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
$env:Path = "$env:QTDIR\bin;$env:Path"
.\build\windows-debug\eda-p2p.exe --name Anna --tcp-port 50001
.\build\windows-debug\eda-p2p.exe --name Ben  --tcp-port 50002
```

### Windows/macOS/Linux – Qt Creator

*Datei → Datei oder Projekt öffnen…* → `CMakeLists.txt` → Kit „Desktop Qt 6.x“ wählen →
*Ausführen* (Strg+R). Kommandozeilenargumente: *Projekte → Ausführen → Kommandozeilenargumente*.

### macOS

```sh
export PATH="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin:$HOME/Qt/Tools/Ninja:$PATH"
export QTDIR="$HOME/Qt/6.12.0/macos"
cmake --preset debug
cmake --build --preset debug
ctest --preset debug

# Zwei Instanzen:
open -n build/debug/eda-p2p.app --args --name Anna --tcp-port 50001
open -n build/debug/eda-p2p.app --args --name Ben  --tcp-port 50002
```

Beim ersten Start fragt macOS: *„EDA P2P möchte Geräte im lokalen Netzwerk finden“* →
**Erlauben**. Ohne diese Erlaubnis schlägt der Broadcast mit „No route to host“ fehl
(Details in [docs/testplan.md](docs/testplan.md)).

### Linux

```sh
export PATH="$HOME/Qt/Tools/CMake/bin:$HOME/Qt/Tools/Ninja:$PATH"
export QTDIR="$HOME/Qt/6.12.0/gcc_64"
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/eda-p2p --name Anna --tcp-port 50001 &
./build/debug/eda-p2p --name Ben  --tcp-port 50002 &
```

Ubuntu/Debian brauchen evtl. `sudo apt install libgl1-mesa-dev libxkbcommon-dev libxcb-cursor0`.

## Bedienung

| Kommandozeilenoption | Standard | Bedeutung |
|---|---|---|
| `-n, --name` | Rechnername | Anzeigename |
| `-u, --udp-port` | 45454 | Broadcast-„Kanal“ – **bei allen gleich** |
| `-t, --tcp-port` | 45455 | eigener Port für Direktverbindungen – **pro Instanz verschieden**. Ist er belegt, wählt die App automatisch einen freien und zeigt ihn oben an. |

1. **Broadcast:** unten links ist *An alle (UDP)* gewählt → Text eintippen → *Senden* (oder Enter).
   Nachrichten an alle erscheinen **grün**.
2. **Direktverbindung:** oben *Meine IP* und *TCP* des anderen ablesen → links bei
   *Direktverbindung (TCP)* `IP:Port` eintragen → *Verbinden*. Auf demselben PC: `127.0.0.1:50002`.
3. **Direktnachricht:** links auf den Namen klicken (oder unten *Direkt: …* wählen) → Text → *Senden*.
   Direktnachrichten erscheinen **orange**.

## Projektstruktur

```
CMakeLists.txt        Build: Bibliothek p2pcore, App eda-p2p, Tests
CMakePresets.json     gemeinsame Build-Konfiguration (VS, Qt Creator, Kommandozeile)
packaging/            macOS Info.plist (Bundle-ID, Text für die Erlaubnis „Lokales Netzwerk“)
qml/Main.qml          Oberfläche
src/main.cpp          Start: Kommandozeile, Controller, QML-Engine
src/core/             Message (JSON-Format), FrameCodec (TCP-Framing) – ohne Netzwerk
src/network/          UdpService, TcpService, PeerConnection – ohne GUI
src/app/              ChatController, ChatLogModel – Brücke C++ ↔ QML
tests/                Qt-Test-Programme
docs/                 Dokumentation
```

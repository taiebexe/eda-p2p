# Testplan und Fehlersuche

## 0. Automatische Tests (vor jedem Push)

```sh
cmake --build --preset debug && ctest --preset debug          # macOS/Linux
cmake --build --preset windows-debug; ctest --preset windows-debug   # Windows
```

Erwartet: `100% tests passed`. Beim UDP-Test ist **SKIP** erlaubt, wenn das System
keinen Broadcast zulässt (siehe Fehlersuche). Die Begründung steht in der Ausgabe.

## 1. Zwei Instanzen auf einem PC

| # | Schritt | Erwartet | ✓ |
|---|---|---|---|
| 1.1 | Instanz A starten: `--name Anna --tcp-port 50001` | Fenster; Chatverlauf: „UDP-Broadcast aktiv auf Port 45454, sende an: …“ (die Broadcast-Adresse eures Netzes, z. B. `192.168.0.255`), „Warte auf TCP-Verbindungen auf Port 50001“ | |
| 1.2 | Instanz B starten: `--name Ben --tcp-port 50002` | dasselbe mit Port 50002 | |
| 1.3 | In A unten *An alle (UDP)* → „Hallo von Anna“ → *Senden* | B zeigt eine **grüne** Blase links: „Anna · an alle“. A zeigt die Nachricht **genau einmal** rechts (kein Echo). | |
| 1.4 | In B antworten | A empfängt | |
| 1.5 | In B links `127.0.0.1:50001` → *Verbinden* | Beide: „TCP-Verbindung … hergestellt“, „… ist jetzt direkt verbunden“. Links unter *Verbunden (1)* steht der Name des anderen. | |
| 1.6 | In B links auf „Anna“ klicken → Text → *Senden* | A zeigt eine **orange** Blase: „Ben · direkt“ | |
| 1.7 | In A Direktnachricht an Ben | B empfängt | |
| 1.8 | Instanz A schließen | B: „Verbindung mit Anna getrennt.“ *Verbunden (0)*, unten steht „Empfänger wählen …“. **Keine** Fehlermeldung. | |
| 1.9 | In B `127.0.0.1:50001` erneut verbinden (A ist zu) | Fehlermeldung „Connection refused“, App bleibt bedienbar | |
| 1.10 | In B `10.255.255.1:50001` verbinden (nicht erreichbar) | GUI bleibt bedienbar, nach 5 s „Zeitüberschreitung“ | |
| 1.11 | In B eigene Adresse `127.0.0.1:50002` | „Das ist die eigene Instanz – Verbindung wird getrennt.“ | |
| 1.12 | In B `hallo` → *Verbinden* | Hinweis auf das Format IP:Port | |
| 1.13 | Zweite Instanz **ohne** `--tcp-port` starten, während die erste läuft | Meldung „Port belegt“ + „Ausweich-Port gewählt“; oben wird der neue Port angezeigt | |
| 1.14 | Umlaute/Emoji senden („Grüße 🚀“) | Kommen unverändert an | |

## 2. Zwei PCs im selben LAN

Vorbereitung: beide im **selben** WLAN/Netz. Am besten ein **Handy-Hotspot**
(Uni-WLAN blockiert oft Broadcast, siehe unten).

| # | Schritt | Erwartet | ✓ |
|---|---|---|---|
| 2.1 | PC 1 und PC 2 starten (Standardports reichen) | „Meine IP“ zeigt je eine Adresse im selben Netz, z. B. 172.20.10.x | |
| 2.2 | PC 1 Broadcast | PC 2 empfängt | |
| 2.3 | PC 2 Broadcast | PC 1 empfängt | |
| 2.4 | PC 2: IP von PC 1 + `:45455` → *Verbinden* | Beide verbunden, Namen sichtbar | |
| 2.5 | Direktnachrichten in beide Richtungen | kommen an | |
| 2.6 | WLAN an PC 1 ausschalten | PC 2 meldet Trennung (kann bis zu einige Minuten dauern – TCP merkt Abbrüche ohne Datenverkehr spät) | |
| 2.7 | Windows ↔ macOS gemischt | wie 2.2–2.5 | |

## 3. Fehlersuche

### Windows-Firewall
Beim ersten Start fragt Windows: *„Windows Defender Firewall hat einige Funktionen blockiert“*.
→ **Private Netzwerke** anhaken, *Zugriff zulassen*.
Wurde das weggeklickt: *Windows-Sicherheit → Firewall- & Netzwerkschutz → Zugriff von Apps durch die
Firewall zulassen* → `eda-p2p.exe` für *Privat* erlauben.
Prüfen: Ist das WLAN als **Öffentlich** eingestuft, blockiert Windows eingehende Verbindungen strenger →
*Einstellungen → Netzwerk → WLAN → Eigenschaften → Privates Netzwerk*.
Notfall-Test (nur kurz!): Firewall testweise aus, testen, wieder an.

### macOS: „Broadcast fehlgeschlagen: Unable to send a message“
Das war unser eigener Fehler in der ersten Version: Sie sendete an `255.255.255.255`, und macOS
antwortet darauf zeitweise mit „No route to host“. Seit der Korrektur sendet die App an die
Broadcast-Adresse jeder Netzwerkkarte (steht beim Start im Verlauf: „sende an: …“).
Kommt die Meldung trotzdem, mit der Adresse in Klammern:
→ Fragt macOS „… Geräte im lokalen Netzwerk finden?“ → **Erlauben**. Nachträglich:
  *Systemeinstellungen → Datenschutz & Sicherheit → Lokales Netzwerk* → **EDA P2P** einschalten.
→ *Systemeinstellungen → Netzwerk → Firewall* → eingehende Verbindungen für EDA P2P erlauben.

### Uni-WLAN (eduroam) blockiert Broadcast / isoliert Geräte
Viele Firmen- und Uni-WLANs haben **Client-Isolation**: Geräte sehen sich gegenseitig nicht,
Broadcasts werden verworfen. Symptom: Auf einem PC mit zwei Instanzen geht alles, zwischen zwei PCs
nichts, auch kein TCP.
→ **Handy-Hotspot** aufmachen, beide Laptops verbinden. Für die Demo von vornherein einplanen.
→ Alternative: beide Laptops per LAN-Kabel an einen kleinen Switch/Router.

### Falsche Netzwerkkarte
`QHostAddress::Broadcast` (255.255.255.255) geht je nach Betriebssystem nur über **eine** Karte –
meist die mit der Standardroute. Aktive **VPNs**, **VirtualBox/Hyper-V/WSL/Docker**-Adapter oder
LAN + WLAN gleichzeitig können dazu führen, dass der Broadcast im falschen Netz landet.
Deshalb sendet die App an die Broadcast-Adresse **jeder** aktiven Karte (`QNetworkInterface`).
Beim Start steht im Verlauf „sende an: …“ – dort muss das gemeinsame Netz dabei sein
(z. B. `172.20.10.15` beim iPhone-Hotspot, `192.168.43.255` bei vielen Android-Hotspots).
→ Fehlt es: VPN trennen, unnötige Adapter deaktivieren, nur **ein** Netzwerk aktiv lassen.
→ „Meine IP“ in der App zeigt alle IPv4-Adressen. Nehmt für die TCP-Verbindung die aus dem gemeinsamen Netz.
→ Windows prüfen: `ipconfig`, macOS/Linux: `ifconfig` bzw. `ip addr`.

### Port belegt
Symptom: „TCP-Port 45455 konnte nicht geöffnet werden: The bound address is already in use“.
Die App weicht beim TCP-Port automatisch aus und zeigt den neuen Port an. Ansonsten:
→ anderen Port mit `--tcp-port 50003` wählen, oder herausfinden, wer ihn belegt:
- Windows: `netstat -ano | findstr :45455` → PID im Task-Manager
- macOS/Linux: `lsof -nP -i :45455`

Der UDP-Port darf (gewollt) von mehreren Instanzen geteilt werden. Belegt ihn ein **anderes**
Programm ohne Teilen-Option, mit `--udp-port 45460` ausweichen – dann **auf allen Rechnern**.

### Nachrichten kommen nur in eine Richtung an
Fast immer Firewall auf der **empfangenden** Seite oder unterschiedliche UDP-Ports.

### Schnelltest ohne unsere App
- TCP erreichbar? Windows: `Test-NetConnection 192.168.0.42 -Port 45455` · macOS/Linux: `nc -vz 192.168.0.42 45455`
- Wireshark mit Filter `udp.port == 45454` zeigt, ob Broadcasts überhaupt das Netz erreichen.

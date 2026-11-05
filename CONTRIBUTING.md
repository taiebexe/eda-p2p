# Zusammenarbeit im Team

## Branches

- `main` ist **immer lauffähig**: baut und alle Tests sind grün. Niemand pusht direkt auf `main`.
- Pro Aufgabe ein Branch von `main`:
  - `feature/<thema>` – neue Funktion, z. B. `feature/profile-broadcast`
  - `fix/<thema>` – Fehlerbehebung, z. B. `fix/udp-wrong-interface`
  - `docs/<thema>` – nur Dokumentation
- Branches klein und kurzlebig halten (wenige Tage). Lieber zwei kleine PRs als einen großen.

## Pull Requests

1. Vor dem PR: `git pull --rebase origin main`, bauen, `ctest` ausführen.
2. PR auf GitHub öffnen: **Was** wurde geändert, **warum**, **wie getestet** (z. B. „Testplan 1.1–1.8“).
3. **Mindestens ein** anderes Teammitglied reviewt und genehmigt. Wer reviewt, muss den Code
   erklären können – das ist zugleich Vorbereitung auf Rückfragen des Betreuers.
4. Merge per *Squash and merge* → eine saubere Zeile pro Feature in der Historie.
5. Branch nach dem Merge löschen.

Empfehlung in den GitHub-Einstellungen: *Branch protection* für `main`
(„Require a pull request before merging“, 1 Approval).

## Commits

- Kurze Betreffzeile im Imperativ, z. B. „Add TCP connect timeout“ oder „UDP: Eigene Nachrichten filtern“.
- Eine logische Änderung pro Commit.
- Keine Build-Ordner, keine `CMakeUserPresets.json`, keine IDE-Dateien (siehe `.gitignore`).

## Code-Stil

- C++17, Qt 6, keine veralteten Qt-5-APIs.
- Bezeichner auf **Englisch**, Kommentare und UI-Texte auf **Deutsch**.
- Kommentare erklären das **Warum**, nicht das Was.
- Signale/Slots statt `waitFor…` im GUI-Thread, keine eigenen Threads ohne Absprache.
- Neue Logik in `core/` oder `network/` bekommt einen Test in `tests/`.

## Aufgabenteilung (Vorschlag)

| Person | Bereich | Dateien |
|---|---|---|
| A | Protokoll + Tests | `src/core/`, `tests/` |
| B | Netzwerk | `src/network/` |
| C | Oberfläche + Doku | `src/app/`, `qml/`, `docs/` |

Jeder reviewt die PRs der anderen beiden. So kennt am Ende jeder den ganzen Code.
Aufgaben als **GitHub Issues** anlegen und im **GitHub Project Board** (To do / In progress / Review / Done) verfolgen.
Kurzes Sync 1×/Woche (15 min): Was ist fertig, was blockiert, wer macht was.

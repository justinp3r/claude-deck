# claude-dashboard

Ein physisches Freigabe-Panel für Claude Code. Ein ESP32-S3 mit Breitbild-Touch-LCD
steht neben der Tastatur und beantwortet genau eine Frage:

> **Darf dieser Agent das jetzt tun?**

Statt jede Rückfrage im Terminal wegzuklicken, landet sie auf dem Gerät. Ein Tipp
auf **Accept** oder **Deny** entscheidet, was Claude Code als Nächstes tut.

```
┌──────────┬────────────────────────────┬─────────────┐
│          │ ● Approval requested       │   Accept    │  ← gefüllt, schwarze Schrift
│  Clawd   │ [Bash]                     ├─────────────┤
│          │ git push origin            │    Deny     │  ← umrissen, weiße Schrift
│ backend  │ main --force               │             │
└──────────┴────────────────────────────┴─────────────┘
   640 × 172 px · 84 × 22,6 mm · beide Tasten 22 × 8,7 mm
```

## Stand

Läuft. Die Kette Claude Code → Hook → Bridge → Gerät → Entscheidung ist
nachgewiesen, inklusive aller Ausfallpfade.

## Loslegen

```bash
git clone <repo> claude-dashboard && cd claude-dashboard
./scripts/install.sh
```

Danach Claude Code einmal neu starten. Mehr ist es nicht — vorausgesetzt, die
Firmware ist schon auf dem Gerät und Node ist installiert.

Rückgängig: `./scripts/install.sh --uninstall`

**Firmware neu bauen** (nur nötig, wenn du am Gerät etwas änderst):

```bash
./scripts/setup.sh     # einmalig: arduino-cli, ESP32-Core, LVGL, Fonts
./scripts/flash.sh     # übersetzen und flashen
```

## Wie es zusammenhängt

```
Claude Code                 Bridge (Node)              ESP32-S3
    │                            │                        │
    │ PermissionRequest-Hook     │                        │
    ├───────────────────────────►│  USB-C                 │
    │  (Unix-Socket)             ├───────────────────────►│  zeigt die Anfrage
    │                            │                        │
    │◄───────────────────────────┤◄───────────────────────┤  Accept / Deny
    │  allow / deny              │                        │
```

Der Daemon sitzt dazwischen, weil den seriellen Port immer nur ein Prozess offen
haben kann — und typischerweise laufen mehrere Claude-Code-Sessions. Er kennt
dadurch auch die Warteschlange.

**Es ist USB-C, kein Bluetooth.** Dasselbe Kabel, das das Gerät mit Strom
versorgt, ist die Datenverbindung. Das Gerät hat **kein Claude-Konto** und
spricht nie mit Anthropic — es zeigt an, was die Bridge schickt, und schickt
`allow`/`deny` zurück. Keine Zugangsdaten darauf, nichts übers Netz.

## Die Regel, an der alles hängt

**Jeder Ausfallpfad gibt die Frage ans Terminal zurück.** Kabel gezogen, Bridge
tot, Zeit abgelaufen, kaputtes JSON: der Hook liefert keine Entscheidung, und der
normale Dialog erscheint wie vorher. Gemessen:

| Fall | Dauer bis zum Terminal |
|---|---|
| Kabel gezogen | 0,06 s |
| Bridge gestoppt | 0,06 s |
| Gerät da, niemand tippt | 30 s (einstellbar) |

Es gibt keinen Pfad, auf dem ein stummes Gerät etwas freigibt. Auch nicht mit
einem einzelnen gestörten Messwert — eine Berührung zählt erst, wenn zwei
aufeinanderfolgende Messungen sie bestätigen.

## Was das Gerät zeigt

| Zustand | Inhalt |
|---|---|
| `idle` | laufende Sessions; ist nichts los, spaziert Clawd durchs Bild |
| `approval` | Kachel, Befehl, Accept und Deny |
| `queue` | wie oben, plus `1 of 2 waiting` und Positionsleiste |
| `detail` | der ganze Befehl über die volle Breite |
| `usage` | 5-Stunden-Fenster, Woche, Kontext als Balken |
| `disconnected` | roter Steg, `Bridge unreachable` |

Bedient wird ohne sichtbare Schaltflächen — auf 22,6 mm Höhe ist jeder Knopf
Platz, der dem Inhalt fehlt:

| Geste | Wirkung |
|---|---|
| wischen ↓ | Verbrauchsanzeige (in `idle` erst durch die Sessionliste blättern) |
| wischen ↑ | zurück |
| wischen ← → | zwischen wartenden Anfragen blättern |
| tippen auf die Kachel | Detailansicht, nochmal tippen geht zurück |

## Hardware

**Waveshare ESP32-S3-Touch-LCD-3.49, Revision V2** — 3,49" IPS, 172 × 640 nativ
(hier quer betrieben), AXS15231B über QSPI, Touch über I²C, 16 MB Flash, 8 MB
Octal-PSRAM.

Zwei Einstellungen sind nicht optional, sonst startet das Gerät nicht:
`PSRAM=opi` (nicht `enabled`) und die **V2**-Pinbelegung. Warum, steht in
[CLAUDE.md](CLAUDE.md#hardware).

## Aufbau

```
firmware/claude_dashboard/   Arduino-Sketch: LVGL, UI, Protokoll
bridge/                      Node-Daemon, Hook-Anbindung, Selbsttest
.claude/                     Hook-Registrierung für dieses Projekt
scripts/                     install.sh, setup.sh, flash.sh, bridge-service.sh
lv_conf.h                    LVGL-Konfiguration
agent-panel-design.html      Design-Referenz: sechs Zustände als SVG
SquareLineStudioExport/      erster Entwurf aus SquareLine Studio, nur Referenz
docs/vendor/                 Referenz-Sketch von Waveshare
```

Weder Bridge noch Hook haben npm-Abhängigkeiten.

## Weiterlesen

| | |
|---|---|
| [VERBINDEN.md](VERBINDEN.md) | Einrichtung, zweiter Rechner, warum kein Bluetooth |
| [FLASHEN.md](FLASHEN.md) | Firmware bauen, ausprobieren, Fehlerbilder |
| [CLAUDE.md](CLAUDE.md) | Architektur, Design-Entscheidungen, alle Fallstricke |

## Selbsttest

```bash
./scripts/flash.sh -t          # Firmware mit Testschalter
node bridge/selftest.mjs       # Accept, Deny und Zeitablauf prüfen
./scripts/flash.sh             # danach wieder ohne Schalter
```

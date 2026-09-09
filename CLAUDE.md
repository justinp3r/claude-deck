# claude-dashboard

Ein physisches Freigabe-Panel für Claude Code: ein ESP32-S3 mit Breitbild-Touch-LCD
steht neben der Tastatur und beantwortet genau eine Frage — *darf dieser Agent das jetzt tun?*

Statt jede Rückfrage im Terminal wegzuklicken, landet sie auf dem Gerät.
Accept/Deny dort entscheidet, was Claude Code als Nächstes tut.

## Stand

Die Kette steht: Claude Code → Hook → Bridge → Gerät → Entscheidung zurück.
Nachgewiesen mit `bridge/selftest.mjs` (Accept, Deny, Zeitablauf).
Die Demo auf dem Gerät läuft nur noch, solange sich die Bridge nicht gemeldet hat.

Einrichten und verbinden: [VERBINDEN.md](VERBINDEN.md).

**Sprache:** Alle Texte auf dem Display sind Englisch. Kommentare im Code und
diese Dokumentation sind Deutsch.

## Bauen und flashen

```bash
./scripts/setup.sh     # einmalig
./scripts/flash.sh     # übersetzen + flashen   (-m Monitor, -c nur übersetzen)
```

Details und Fehlerbilder: [FLASHEN.md](FLASHEN.md).

## Hardware

**Waveshare ESP32-S3-Touch-LCD-3.49, Revision V2** (Silkscreen `Rev1.1`, QC-Aufkleber `V2`).

| | |
|---|---|
| MCU | ESP32-S3**R8**, Xtensa LX7 Dual-Core, 240 MHz |
| Flash | 16 MB (W25Q128JVSI, Quad) |
| PSRAM | 8 MB **Octal** — das `R8` im Namen heißt Octal, nicht Quad |
| Display | 3,49" IPS, 172 × 640 nativ, hier als 640 × 172 quer betrieben |
| Display-Treiber | AXS15231B über QSPI |
| Touch | AXS15231B, I²C, Adresse `0x3b` |
| GPIO-Expander | TCA9554PWR — hängt der LCD-Reset dran, nicht an einem GPIO |
| Sonst | QMI8658 (IMU), PCF85063 (RTC), ES8311/ES7210 (Audio) |
| USB | natives USB-Serial/JTAG (VID `0x303A`, PID `0x1001`) |

### Zwei Fallen, die beide zu einem toten Gerät führen

**PSRAM muss `opi` sein, nicht `enabled`.** `PSRAM=enabled` bedeutet in der Arduino-
Toolchain QSPI-PSRAM. Der R8 hat Octal. Mit der falschen Einstellung meldet der
Bootloader `quad_psram: PSRAM chip is not connected` und das Board hängt in einer
Boot-Schleife, weil `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` für den Framebuffer
NULL liefert und das Assert in `lvgl_port.c` zieht.

**V1-Pins passen auf V2 nicht.** Gegenüber V1 sind zwei Paare getauscht:

| | V1 | V2 |
|---|---|---|
| Backlight | GPIO 8 | **GPIO 42** |
| LCD-Reset | GPIO 21 | **kein GPIO — TCA9554 Bit 5** |
| LCD-TE | (ungenutzt) | GPIO 21 |
| EXIO_INT | (ungenutzt) | GPIO 8 |

Das GitHub-Repo `waveshareteam/ESP32-S3-Touch-LCD-3.49` **ohne** `-V2` im Namen ist
die V1-Variante (letzter Stand Februar 2026, V2 kam im Juni 2026). Richtige Quelle:
[ESP32-S3-Touch-LCD-3.49-V2](https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49-V2),
Beispiel `Arduino/examples/10_LVGL_V9_Test`.

### Der Touch: vier Fehler übereinander

Der Touch funktionierte anfangs überhaupt nicht, und zwar aus vier unabhängigen
Gründen. Jeder einzelne hätte gereicht.

**1. Falscher I²C-Bus.** `i2c_bsp.c` legt den Touchcontroller auf
`user_i2c_port1_handle` (Zeile 51), aber das Waveshare-Beispiel liest ihn mit
`i2c_master_write_read_dev`, das auf **Port 0** wartet. Ergebnis: gestörte Frames,
also Phantomberührungen an zufälligen Stellen. Richtig ist
`i2c_master_touch_write_read`. **Das ist ein Fehler im Vendor-Beispiel.**

**2. Jeder Tipp wurde zur Wischgeste.** Das Panel meldet während einer ruhigen
Berührung um einige Pixel schwankende Werte. LVGL summiert *jede* Bewegung auf
und erkennt ab 50 px eine Geste — nach ein paar Messungen ist die Schwelle
erreicht, der Klick fällt aus. Gegenmittel: eine Totzone von 8 px, unterhalb
derer der gemeldete Punkt stehen bleibt.

**3. Ein einzelner Störframe konnte freigeben.** Auf einem Gerät, das Freigaben
erteilt, ist das der gefährlichste denkbare Fehler. Gegenmittel: eine Berührung
zählt erst, wenn **zwei aufeinanderfolgende Messungen** sie bestätigen und dabei
innerhalb von 40 px liegen. Nachgemessen: 90 s mit offener Anfrage, null
Phantome, null selbsttätige Entscheidungen.

**4. Jedes Deko-Rechteck fing die Berührung ab.** `lv_obj_create` setzt
`LV_OBJ_FLAG_CLICKABLE` von sich aus (`lv_obj.c`: `obj->flags =
LV_OBJ_FLAG_CLICKABLE`), und der Treffertest liefert immer das **oberste**
getroffene Kind. Clawds Rumpf schluckte damit die Tipps, die ihm galten.
Deshalb schalten `bare()` und `block()` das Flag wieder ab, und nur echte
Bedienelemente bekommen es gezielt gesetzt.

### LVGL-Heap liegt im PSRAM

`LV_USE_STDLIB_MALLOC` steht auf `LV_STDLIB_CLIB`, nicht auf `BUILTIN`.
Ursprünglich nötig, weil LVGL transformierte Objekte in eine eigene Ebene
rendert und der statische Pool im internen RAM dafür zu klein war — LVGL wartete
dann in `lv_draw_dispatch` endlos auf die Ebene und der **Task-Watchdog** schlug
zu. Die Transformationen sind inzwischen wieder draußen, die Einstellung bleibt
trotzdem: die globalen Variablen liegen damit bei **28 KB statt 208 KB** (8 %
statt 63 % des internen RAM), und große Puffer landen im PSRAM statt eng zu
werden.

### Zwei Fallen auf der Softwareseite

**USB-CDC-Empfangspuffer.** Default sind 256 Byte, und `HWCDC::begin()` setzt den
Wert nur, wenn er nicht schon vorgegeben ist. Eine Freigabeanfrage mit vollem
Befehl ist leicht über 300 Byte und kommt in einem Rutsch an — sie ging still
verloren. Deshalb steht `Serial.setRxBufferSize(4096)` **vor** `Serial.begin()`.

**`build.extra_flags` niemals überschreiben.** Beim ESP32-Core stehen dort
`ARDUINO_USB_CDC_ON_BOOT` und die USB-Modus-Defines. Wer die Eigenschaft für ein
eigenes `-D` überschreibt, bekommt ein Board, das bootet und stumm bleibt.
Eigene Defines gehören in `compiler.cpp.extra_flags` (siehe `flash.sh -t`).

**`arduino-cli upload` baut nicht neu.** Immer `compile --upload` in einem Aufruf,
sonst landet ein älteres Binary auf dem Board. `flash.sh` macht das so.

## Entwicklungsumgebung

MacBook Air M2, macOS Darwin 25.6. arduino-cli 1.5.1, ESP32-Core 3.3.11, LVGL 9.3.0,
Node 24, Python 3.13. Board hängt an `/dev/cu.usbmodem101`.

**ctags auf Apple Silicon.** Das mit Arduino gelieferte `ctags` ist ein x86-Binary;
ohne Rosetta bricht jeder Build mit `bad CPU type in executable` ab. Rosetta lässt
sich hier nicht ohne Passwort installieren, deshalb liegt unter
`~/Library/Arduino15/packages/builtin/tools/ctags/5.8-arduino11/ctags` ein Symlink
auf `universal-ctags` (Original als `ctags.x86_64.bak` daneben). `setup.sh` macht das.

**Warum `main.cpp` und eine leere `.ino`.** Nur die `.ino` läuft durch den
Arduino-Präprozessor, der per ctags Funktionsprototypen einfügt. universal-ctags
meldet Zeilennummern um eins versetzt und ein anderes `typeref`-Format, wodurch
Prototypen ohne Rückgabetyp mitten in `setup()` landen. Steht der Code in einer
`.cpp`, passiert das nicht. Die `.ino` muss nur existieren und bleibt leer.

## Architektur

```
Claude Code (Mac)
  └── PermissionRequest-Hook ──►  Bridge-Daemon (Node, Mac)
        .claude/hooks/…mjs          └── USB CDC Serial ──►  ESP32-S3
                              ◄── allow / deny  ◄────────────┘
```

**`PermissionRequest`, nicht `PreToolUse`.** Der Unterschied entscheidet über
brauchbar oder unbenutzbar: `PreToolUse` feuert vor *jedem* Werkzeugaufruf, auch
bei jedem `Read` und `Grep` — man müsste die ganze Berechtigungslogik nachbauen.
`PermissionRequest` feuert genau dann, wenn im Terminal der Dialog käme.

Antwortformat des Hooks:
```json
{"hookSpecificOutput":{"hookEventName":"PermissionRequest",
  "decision":{"behavior":"allow"}}}
```
Keine Ausgabe und Exit 0 heißt „keine Entscheidung" — der normale Ablauf greift
und das Terminal fragt. Genau darauf beruht die Ausfallsicherheit.

**Hook statt Agent SDK:** funktioniert mit der normalen `claude`-CLI und jeder
Session, die ohnehin läuft. Das Agent SDK (`canUseTool`) wäre erst nötig, wenn die
Bridge Sessions auch selbst *starten* soll.

**Daemon dazwischen, nicht Hook direkt am Port:** den seriellen Port kann nur ein
Prozess offen haben, und es laufen typischerweise mehrere Sessions. Der Daemon
serialisiert das und kennt dadurch die Warteschlange.

**Kein npm.** Bridge und Hook sind reines Node: `stty` stellt den Port ein, danach
wird er als Datei gelesen. Gelesen wird mit nicht-blockierendem `readSync` im
Intervall — `fs.createReadStream` auf einem TTY blockiert in Node den Event-Loop.

**USB-Kabel vor BLE:** Der Port ist da, versorgt das Board und hat die niedrigste
Latenz. Der S3 kann nur BLE, kein Classic; Node-BLE auf macOS ist eine eigene
Fehlerquelle. Beim Flashen muss die Bridge den Port freigeben.

### Sicherheitsregel (nicht verhandelbar)

Timeout, Kabel ab, Bridge tot, Firmware-Crash — **jeder** Ausfallpfad gibt `ask`
zurück und die Frage landet im Terminal. Ein stummes Gerät darf nie ein zustimmendes
sein. Kein Default-Allow, nirgends.

## Aufbau

```
.claude/
  settings.json          Hook-Registrierung fuer dieses Projekt
  hooks/permission-request.mjs
bridge/
  bridge.mjs             Daemon: serieller Port + Unix-Socket
  statusline.sh          Vorschalter, holt die Limits aus der Statusline
  settings-patch.mjs     traegt Hook und Statusline ein/aus, mit Sicherung
  selftest.mjs           prueft Accept, Deny und Zeitablauf durch die ganze Kette
firmware/claude_dashboard/
  claude_dashboard.ino   leer, siehe oben
  main.cpp               setup()/loop(), Demo-Zyklus, Serial-Kommandos
  protocol.cpp/.h        Zeilenprotokoll zur Bridge (ArduinoJson)
  clawd.c/.h             das animierte Maskottchen aus Rechtecken
  ui_panel.c/.h          die fünf Zustände
  ui_theme.h             Farben, Maße, Schriftgrößen aus dem Design
  ui_font_*.c            generierte Schriften (siehe unten)
  lvgl_port.c/.h         Waveshare-Port, angepasst: Demo raus, Lock nach außen
  user_config.h          V2-Pinbelegung
  i2c_bsp.c/.h, src/     unverändert aus dem Waveshare-V2-Beispiel
lv_conf.h                LVGL-Konfiguration, wird nach ~/Documents/Arduino/libraries/ kopiert
scripts/                 install.sh (Einrichtung), setup.sh (Toolchain),
                         flash.sh, bridge-service.sh, common.sh (FQBN)
docs/vendor/             Referenz-Sketch von Waveshare
SquareLineStudioExport/  erster SquareLine-Entwurf, nur noch Referenz
agent-panel-design.html  Design-Referenz
```

## Design

`agent-panel-design.html` ist die Referenz — die Zustände als SVG bei exakt
640 × 172 px, mit der Maßrechnung dazu (194 ppi, 7,62 px/mm).

Gebaut sind sechs: `idle` · `approval` · `queue` · `detail` · `usage` · `disconnected`.

### Navigation ohne Bedienelemente

Auf 22,6 mm Höhe ist jeder Knopf Platz, der dem Inhalt fehlt. Deshalb Gesten:

| Geste | von | nach |
|---|---|---|
| wischen ↑ | `idle` mit >3 Sessions | ein Blatt weiter in der Liste |
| wischen ↓ | `idle`, Liste nicht oben | ein Blatt zurück |
| wischen ↓ | sonst überall | `usage` |
| wischen ↑ | `usage` | zurück, woher man kam |
| wischen ← / → | `approval`, `queue` | nächste / vorige offene Anfrage |
| tippen auf die Kachel | `approval`, `queue` | `detail` |
| tippen irgendwo | `detail` | zurück |

Das Modell dahinter: **senkrecht wechselt den Modus** (Gerät statt Anfrage),
**waagerecht blättert unter Gleichrangigem** (die wartenden Anfragen),
**tippen geht eine Ebene tiefer und wieder heraus**.

Die Sessionliste hat Vorrang vor der Verbrauchsanzeige: Wischen nach unten
blättert erst zurück und gibt die Geste erst frei, wenn die Liste oben steht —
so wie man eine Liste über den Anfang hinauszieht. Drei Zeilen sind sichtbar
(je 34 px, 112 px Blatt), bis zu neun werden gehalten. Ab dem zweiten Blatt
erscheint rechts ein 3 px schmaler Positionssteg — Information, kein
Bedienelement, dieselbe Sprache wie die Warteschlangenleiste im Design.

Eine Sessionzeile ohne Namen wird komplett ausgeblendet, Punkt inklusive.
Sonst stünde bei null Sessions ein Punkt ohne Text da, als liefe etwas
Namenloses.

Ist die Liste leer, spaziert Clawd (140 px breit) durch die Fläche: 13 s von
links nach rechts, dann 5 s Pause außerhalb des Bildes. Sonst tut er nichts.

Es gab einmal fünf Regungen auf Tipp (hüpfen, drehen, wackeln, winken, nicken).
Wieder entfernt: die antippbare Fläche lag genau dort, wo man wischt, und das
Herumspringen lenkte ab. Die Ruheansicht reagiert jetzt nur noch auf Wischen. Die Beinanimation aus
`clawd.c` läuft dabei ohnehin, es ist also wirklich ein Gang. Sobald eine
Session da ist, verschwindet er — der Platz gehört dann der Liste. Auch ohne
Bridge ist das der Startzustand, ein frisch geflashtes Gerät zeigt also
`no sessions` und den Spaziergang statt einer leeren Fläche.

**Die Geste darf nie als Tipp durchgehen.** LVGL unterdrückt den Klick bei einer
erkannten Geste *nicht* — ein Wisch, der auf `Accept` beginnt, würde also
zusätzlich freigeben. Deshalb setzt `gesture_cb` ein Flag, das jedes antippbare
Element bei `LV_EVENT_PRESSED` zurücksetzt und vor `LV_EVENT_CLICKED` prüft.
Die Wischschwelle bleibt bei LVGLs 50 px: auf 172 px Höhe ist das gut ein
Drittel, also nichts, was einem beim Griff zur Taste passiert.

Regeln, die aus dem Format folgen:
1. Alles Antippbare ≥ 66 px (8,7 mm). In der Höhe passen genau zwei Reihen.
2. Ein Zustand pro Bild — Ruhe zeigt keine Tasten, Freigabe keine Sessionliste.
3. Bernstein = wartet, Türkis = läuft, Rot = kaputt.
4. Der Ausfall gibt nichts frei.

### Die beiden Tasten

Beide tragen denselben Akzent `#D97757` (Anthropic-Orange). Unterschieden wird
über das Gewicht, nicht über den Farbton:

| | Fläche | Rand | Schrift |
|---|---|---|---|
| **Accept** | gefüllt `#D97757` | — | schwarz |
| **Deny** | keine | 2 px `#D97757` | weiß |

Das Design hatte ursprünglich eine zweite Freigabe-Variante, bei der Accept einen
Wisch über 100 px verlangte, damit der Aufwand der Konsequenz folgt statt der
Symmetrie. Diese Variante ist auf Wunsch entfernt; sie war allein durch die
Wischgeste definiert und wäre ohne sie eine exakte Kopie der ersten gewesen.
Damit bleibt der Einwand des Designs bestehen: beide Aktionen kosten gleich viel,
obwohl nur eine `--force` pusht. Der Abstand von 20 px zwischen den Tasten ist
das Einzige, was einen Fehlgriff verhindert.

### Clawd

Das Maskottchen in der Kachel ist **nicht** das Bitmap aus dem SquareLine-Export
(400 × 400, I8, 161 KB), sondern in [clawd.c](firmware/claude_dashboard/clawd.c)
aus neun Rechtecken nachgebaut. Die Vorlage ist Pixel-Art und lässt sich exakt
vermessen — als Bild könnte sich nur der ganze Kerl bewegen, so läuft jedes Bein
einzeln. Kostet nichts an Flash.

Maße stammen aus dem Original, bezogen auf die Bounding-Box (x 12..389,
y 91..343, also 378 × 253). Grundanimationen: wippen (2 px, 840 ms), Beine
gegenphasig (±3 px Länge, 420 ms), blinzeln (70 ms, alle 3,4 s).



### Das Gerät behält die selbst gewählte Ansicht

Die Bridge schickt den Ruhezustand regelmäßig erneut. Würde das Gerät daraufhin
stur auf `idle` schalten, wäre die Verbrauchsanzeige nach spätestens vier
Sekunden wieder weg — mitten im Lesen. Deshalb aktualisiert `apply_idle` die
Daten immer, wechselt die Ansicht aber nicht, solange `usage` oder `detail`
offen ist. Eine echte Freigabeanfrage darf weiterhin dazwischenfunken; die ist
dringend. Die Bridge sendet den Ruhezustand zusätzlich nur noch bei echter
Änderung.

### Verbrauchsanzeige

`rate_limits.five_hour` und `.seven_day` liefert Claude Code **ausschließlich an
die Statusline** — es gibt keinen CLI-Befehl und keinen Hook dafür. Deshalb hängt
`bridge/statusline.sh` als Vorschalter davor, reicht die Daten per `nc -U` an die
Bridge und ruft danach die ursprüngliche Statusline unverändert auf. Kein
Node-Prozess pro Render, die Statusline darf nicht langsamer werden.

Die Felder fehlen, solange keine Pro-/Max-Anmeldung vorliegt oder noch keine
Antwort in der Session kam; jedes Fenster kann einzeln fehlen. Der Screen zeigt
dann einen Strich statt einer erfundenen Null.

### Schriften

Die eingebauten Montserrat-Fonts von LVGL decken nur ASCII 32–126 ab: **keine
Umlaute, kein `·`, kein `−`** — und LVGL bringt gar keine Monospace mit, die das
Design durchgehend braucht. Deshalb sind die Schriften aus IBM Plex generiert
(die Familie aus dem Design), mit Latin-1-Ergänzungen:

```bash
npx lv_font_conv@1.5.3 --font IBMPlexMono-Regular.ttf \
  -r '0x20-0x7E,0xB0,0xB7,0xC4,0xD6,0xDC,0xDF,0xE4,0xF6,0xFC,0x2212' \
  --size 21 --bpp 4 --format lvgl --force-fast-kern-format --no-compress \
  --lv-include lvgl.h -o ui_font_mono_21.c
```

Vorhanden: Mono 14/16/18/21/30, Sans 17, Sans SemiBold 23/26.
Neue Größe gebraucht? Gleiches Kommando, dann `LV_FONT_DECLARE` in `ui_panel.c`.

### Positionierung

Die SVGs im Design geben Text über die **Grundlinie** an, LVGL über die **obere
Kante**. Statt Grundlinien umzurechnen (bricht bei jedem Fontwechsel) liegen
Textstapel in Flex-Containern mit `LV_FLEX_ALIGN_CENTER`. Das zentriert optisch
korrekt, unabhängig von den Metriken.

Alle Textlabels, die Fremddaten anzeigen, haben feste Breite und
`LV_LABEL_LONG_DOT` — echte Bash-Befehle sind länger als die Spalte und würden
sonst unter die Tasten laufen. Die volle Zeile zeigt die Detailansicht.

## Offene Punkte

- Der Ruhezustand zeigt Sessions, die die Bridge aus den Hook-Aufrufen kennt.
  Zwischen zwei Anfragen weiß sie nichts Neues — eine Session, die gerade nur
  denkt, sieht dort alt aus.

- Touch-Achsen sind nicht auf echter Hardware verifiziert. Falls Tasten spiegel-
  verkehrt reagieren: `TouchInputReadCallback` in `lvgl_port.c`, dort wird
  `pointX`/`pointY` gemappt.
- Clawd-Bild aus dem SquareLine-Export (400 × 400, wird auf ~70 px skaliert) neu
  exportieren, falls es zurück ins UI soll.

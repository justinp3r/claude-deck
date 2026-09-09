# Flashen

Alles ist bereits eingerichtet und **die Firmware läuft schon auf deinem Board**.
Diese Anleitung brauchst du, wenn du etwas geändert hast oder neu anfängst.

## Kurzfassung

```bash
cd ~/Dev/claude-dashboard
./scripts/flash.sh
```

Das war's. Übersetzen, Board suchen, flashen.

Beim allerersten Mal auf einem neuen Rechner vorher einmal:

```bash
./scripts/setup.sh
```

## Was du danach sehen solltest

Das Display zeigt eine Demo, die alle sechs Zustände durchläuft — alle 4 Sekunden
einer weiter. Alle Texte auf dem Gerät sind Englisch:

| | Zustand | Was zu sehen ist |
|---|---|---|
| 1 | **idle** | bis zu drei Sessions; läuft nichts, spaziert Clawd durchs Bild |
| 2 | **approval** | Kachel mit Clawd, Befehl, **Accept** gefüllt oben, **Deny** umrissen unten |
| 3 | **queue** | zwei Balken links, `1 of 2 waiting` in Bernstein |
| 4 | **detail** | ganzer Befehl über die volle Breite, orange Warnung unten |
| 5 | **usage** | drei Balken: 5-Stunden-Fenster, Woche, Kontext |
| 6 | **disconnected** | roter Steg links, `Bridge unreachable` |

Beide Tasten sind im Anthropic-Orange `#D97757`: **Accept** vollflächig mit
schwarzer Schrift, **Deny** nur umrissen mit weißer Schrift.

## Ausprobieren

Mit dem seriellen Monitor kannst du einzelne Zustände festhalten:

```bash
./scripts/flash.sh -m          # flashen und Monitor öffnen
```

Im Monitor tippen:

| Taste | Wirkung |
|---|---|
| `1`–`6` | diesen Zustand zeigen |
| `?` | Hilfe |

Die Demo läuft von selbst, solange sich die Bridge nicht gemeldet hat, und hört
auf, sobald sie da ist — einen Schalter dafür gibt es nicht mehr.

**Touch prüfen:** Zustand `2` wählen, dann auf **Accept** tippen. Im Monitor muss
`{"decision":"allow"}` erscheinen. Bei **Deny** entsprechend `"deny"`.

**Gesten prüfen:** von oben nach unten über den Bildschirm wischen — es erscheint
die Verbrauchsanzeige. Nach oben wischen geht zurück. Laufen mehr als drei
Sessions, blättert dasselbe Wischen erst durch die Liste; rechts zeigt ein
schmaler Steg, wo man gerade ist. Ein Tipp auf die Kachel
links öffnet die Detailansicht, ein weiterer Tipp geht zurück.

**Clawd:** Läuft keine Session, spaziert er durchs Bild — 13 s von links nach
rechts, dann 5 s Pause. Er reagiert nicht auf Tippen; die Ruheansicht gehört
den Wischgesten.

## Wenn etwas nicht stimmt

**Display bleibt schwarz, sonst läuft alles.**
Dann ist die Board-Revision falsch. Dieses Projekt ist auf **V2** eingestellt
(PCB-Silkscreen `Rev1.1`, QC-Aufkleber `V2`). Bei V1 liegen Backlight und
LCD-Reset auf anderen Pins — siehe den Kommentarkopf in
[user_config.h](firmware/claude_dashboard/user_config.h).

**Boot-Schleife, im Monitor steht `PSRAM chip is not connected`.**
Dann steht die PSRAM-Einstellung auf QSPI statt Octal. In
[scripts/common.sh](scripts/common.sh) muss `PSRAM=opi` stehen, nicht `PSRAM=enabled`.
Der ESP32-S3**R8** hat Octal-PSRAM.

**`bad CPU type in executable` beim Übersetzen.**
Das mit Arduino gelieferte `ctags` ist ein x86-Binary und läuft auf dem M2 nicht.
`./scripts/setup.sh` tauscht es gegen ein natives aus.

**Kein Board gefunden.**
Kabel prüfen. Es muss ein Datenkabel sein, kein reines Ladekabel.
`ls /dev/cu.usbmodem*` sollte etwas ausgeben.

**Berührungen kommen nicht an oder das Gerät entscheidet von selbst.**
Sollte nicht mehr vorkommen, aber falls doch: die Touch-Auswertung sitzt in
`TouchInputReadCallback` in [lvgl_port.c](firmware/claude_dashboard/lvgl_port.c).
Dort stehen drei Schutzmaßnahmen mit Begründung — richtiger I²C-Bus, Totzone
gegen Zittern, Bestätigung über zwei Messungen. Zum Nachmessen mit
`--build-property compiler.c.extra_flags=-DTOUCH_DEBUG=1` übersetzen, dann meldet
das Gerät jede Berührung mit Koordinaten über den Monitor.

**Nichts hilft.**
Board mit gedrückter **BOOT**-Taste einstecken (erzwingt den Bootloader),
dann `./scripts/flash.sh`.

## Die Arduino IDE

Du brauchst sie nicht — die Skripte machen alles. Falls du sie trotzdem öffnen
willst: Sketch ist `firmware/claude_dashboard`, und unter *Werkzeuge* müssen
stehen: Board `ESP32S3 Dev Module`, **PSRAM: OPI PSRAM**, Flash Size `16MB`,
Partition `3MB APP/9.9MB FATFS`, **USB CDC On Boot: Enabled**.

Der Code steht bewusst in `main.cpp`, nicht in der `.ino` — Begründung im
Dateikopf von [main.cpp](firmware/claude_dashboard/main.cpp).

# Mit Claude Code verbinden

## Wie es zusammenhängt

```
Claude Code                 Bridge (Node)              ESP32-S3
    │                            │                        │
    │ PermissionRequest-Hook     │                        │
    ├───────────────────────────►│  USB-Kabel             │
    │  (Unix-Socket)             ├───────────────────────►│  zeigt die Anfrage
    │                            │                        │
    │◄───────────────────────────┤◄───────────────────────┤  Accept / Deny
    │  allow / deny              │                        │
```

Drei Teile: ein **Hook**, den Claude Code aufruft, ein **Daemon**, der den
seriellen Port hält, und die **Firmware**. Der Daemon sitzt dazwischen, weil den
Port immer nur ein Prozess offen haben kann — und du hast in der Regel mehrere
Claude-Code-Sessions gleichzeitig. Er kennt dadurch auch die Warteschlange.

**Der Hook ist `PermissionRequest`, nicht `PreToolUse`.** Das ist der Unterschied
zwischen brauchbar und unbenutzbar: `PreToolUse` feuert vor *jedem* Werkzeugaufruf,
also auch bei jedem `Read` und `Grep`, und man müsste die komplette
Berechtigungslogik nachbauen. `PermissionRequest` feuert genau dann, wenn im
Terminal der Dialog erscheinen würde.

## Musst du etwas installieren?

**Zum Benutzen: nein.** Die Bridge und der Hook sind reines Node ohne eine
einzige npm-Abhängigkeit. Node hast du (v24). Der serielle Port wird mit `stty`
eingestellt und dann als normale Datei gelesen.

**Zum Ändern der Firmware:** arduino-cli, ESP32-Core, LVGL, ArduinoJson — das
erledigt `./scripts/setup.sh`.

## Auf einem neuen Mac einrichten

```bash
git clone <dieses-repo> claude-dashboard
cd claude-dashboard
./scripts/install.sh
```

Danach **Claude Code einmal neu starten**. Das war alles.

Das Skript macht drei Dinge, alle umkehrbar:

| | |
|---|---|
| Bridge als Dienst | startet ab jetzt beim Anmelden mit |
| `PermissionRequest`-Hook | trägt sich in `~/.claude/settings.json` ein |
| Statusline-Vorschalter | holt die Limits, ruft deine bisherige Statusline unverändert auf |

Vor jeder Änderung legt es eine Sicherung von `settings.json` an, und deine
bisherige Statusline wird gemerkt, nicht überschrieben. Rückgängig:

```bash
./scripts/install.sh --uninstall
```

**Was du NICHT brauchst:** arduino-cli, die Arduino IDE, oder irgendetwas am
Gerät. Die Firmware bleibt drauf. Das Gerät ist gegenüber dem Rechner
zustandslos — Kabel umstecken genügt.

Der neue Mac braucht nur Node (`brew install node`). Bridge und Hook haben
keine einzige npm-Abhängigkeit.

## Wie hängt das Gerät dran? Und wo meldet man sich an?

**Per USB-C. Kein Bluetooth, kein WLAN, kein Koppeln.** Dasselbe Kabel, das das
Gerät mit Strom versorgt, ist die Datenverbindung.

**Das Gerät hat kein Claude-Konto und braucht keins.** Es spricht nie mit
Anthropic. Angemeldet ist dein Claude Code auf dem Mac, wie vorher auch. Der
Ablauf ist:

1. Claude Code will ein Werkzeug benutzen und braucht deine Erlaubnis
2. Der Hook reicht die Frage an die Bridge auf dem Mac
3. Die Bridge schickt sie über das Kabel ans Gerät
4. Du tippst Accept oder Deny
5. Die Antwort geht denselben Weg zurück

Auf dem Gerät liegen keine Zugangsdaten, es hängt an keinem Netz, und es kann
nichts anderes tun als anzeigen und `allow`/`deny` zurückschicken.

## Bluetooth?

**Kurz: nein, nimm das Kabel.** Länger:

| | USB (jetzt) | BLE | WLAN |
|---|---|---|---|
| Aufwand Mac | keiner | `noble`, native Abhängigkeiten, Berechtigungsdialoge | keiner |
| Mehrere Rechner | umstecken | neu koppeln | gleichzeitig möglich |
| Strom | kommt übers Kabel | Netzteil nötig | Netzteil nötig |
| Latenz | ~1 ms | 30–100 ms | ~5 ms im LAN |
| Wer darf freigeben? | wer am Kabel hängt | wer gekoppelt ist | **jeder im Netz** |

Der ESP32-S3 kann nur BLE, kein Bluetooth Classic. BLE aus Node auf macOS heißt
`@abandonware/noble` — native Kompilierung, Berechtigungsdialoge, und es koppelt
sich an genau einen Rechner. Für „anderer Laptop" löst es das Problem also nicht
besser als ein Kabel, bringt aber eine neue Fehlerquelle mit.

**WLAN wäre der bessere drahtlose Weg**, falls du das Gerät mal frei auf den
Tisch stellen willst: Der S3 hat WLAN, das Gerät hinge am Netz, die Bridge redet
per TCP. Dann brauchst du aber zwingend ein gemeinsames Geheimnis — sonst kann
jeder im selben Netz deine Freigaben erteilen. Das ist die eigentliche Arbeit
daran, nicht die Funkverbindung.

## Ausprobieren

```bash
./scripts/bridge-service.sh log      # in einem Terminal mitlesen
```

Dann in einem anderen Terminal Claude Code in diesem Projekt starten und etwas
tun, das nachfragt. Auf dem Gerät erscheint die Anfrage, im Protokoll steht
`Anfrage r1 Bash "..."`.

Ein vollständiger Selbsttest ohne Finger am Display:

```bash
./scripts/flash.sh -t                # Firmware mit Selbsttest-Schalter
node bridge/selftest.mjs             # Accept, Deny und Zeitablauf prüfen
./scripts/flash.sh                   # danach wieder ohne Schalter!
```

## Wenn nichts passiert

Der Hook ist so gebaut, dass er **im Zweifel nichts tut** — dann fragt einfach
das Terminal wie vorher. Das ist Absicht und kein Fehler. Um zu sehen, woran es
liegt:

| Prüfung | Kommando |
|---|---|
| Läuft die Bridge? | `./scripts/bridge-service.sh status` |
| Was sagt sie? | `./scripts/bridge-service.sh log` |
| Antwortet das Gerät? | im Protokoll steht `Geraet erreichbar` |
| Greift der Hook? | `claude --debug`, dort steht der Hook-Aufruf |

Häufigster Fall: Claude Code fragt gar nicht, weil die Aktion durch eine
Allow-Regel oder den Berechtigungsmodus schon erlaubt ist. Dann feuert
`PermissionRequest` nicht — korrekt so.

## Sicherheit

Der Socket liegt unter `~/.claude-dashboard/bridge.sock` mit Rechten `0600`, nur
dein Benutzer kommt dran.

Der Selbsttest-Schalter `ALLOW_REMOTE_TAP` in der Firmware ist standardmäßig
**aus** und muss es bleiben. Wäre er an, könnte der Rechner sich selbst freigeben
— und genau das soll das Gerät ja verhindern.

**Jeder Ausfallpfad endet im Terminal.** Keine Bridge, kein Gerät, Kabel raus,
Frist abgelaufen, kaputtes JSON: der Hook gibt keine Entscheidung zurück und der
normale Dialog erscheint. Es gibt keinen Pfad, auf dem ein stummes Gerät etwas
freigibt.

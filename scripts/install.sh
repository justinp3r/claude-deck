#!/usr/bin/env bash
# Richtet claude-dashboard auf diesem Mac ein.
#
#   ./scripts/install.sh              einrichten
#   ./scripts/install.sh --uninstall  rueckgaengig machen
#
# Was passiert:
#   1. Bridge als Hintergrunddienst (startet beim Anmelden mit)
#   2. PermissionRequest-Hook in ~/.claude/settings.json
#   3. Statusline-Vorschalter (holt die Limits, ruft deine bisherige auf)
#
# Nicht noetig: arduino-cli, Arduino IDE, irgendetwas auf dem Geraet. Die
# Firmware bleibt drauf, das Geraet ist gegenueber dem Rechner zustandslos.
set -euo pipefail
cd "$(dirname "$0")/.."

say() { printf '\n\033[1m%s\033[0m\n' "$*"; }

if [ "${1:-}" = "--uninstall" ] || [ "${1:-}" = "--purge" ]; then
  say "1/2  Dienst entfernen"
  ./scripts/bridge-service.sh uninstall
  say "2/2  Einstellungen zuruecksetzen"
  node bridge/settings-patch.mjs uninstall

  RUN="$HOME/.claude-dashboard"
  if [ "${1:-}" = "--purge" ]; then
    rm -rf "$RUN"
    say "Auch geloescht: $RUN"
  fi

  say "Fertig."
  cat <<TXT
Es laeuft jetzt nichts mehr: kein Dienst, kein Socket, kein Hook. Hook und
Statusline sind reine Aufruf-Skripte - die starten nur, wenn Claude Code sie
ruft, und beenden sich sofort. Es gab nie einen zweiten Dauerprozess.

Claude Code einmal neu starten, damit die Einstellungen neu gelesen werden.
TXT
  if [ -d "$RUN" ]; then
    cat <<TXT

Liegen geblieben (Daten, nichts Laufendes):
$(ls "$RUN" | sed 's|^|  |')
  in $RUN
Mitloeschen: ./scripts/install.sh --purge
TXT
  fi
  exit 0
fi

say "1/3  Voraussetzungen"
command -v node >/dev/null || { echo "node fehlt. Installieren: brew install node"; exit 1; }
echo "node $(node -v)"
if ! ls /dev/cu.usbmodem* >/dev/null 2>&1; then
  echo "Hinweis: gerade kein Geraet am USB. Einrichten geht trotzdem,"
  echo "         die Bridge findet es, sobald das Kabel steckt."
fi

say "2/3  Bridge als Dienst"
./scripts/bridge-service.sh install

say "3/3  Claude Code verbinden"
node bridge/settings-patch.mjs install

say "Fertig."
cat <<'TXT'
Noch zu tun:
  * Claude Code einmal neu starten (oder /hooks oeffnen), damit die
    Einstellungen gelesen werden. Laufende Sessions nutzen noch die alten.

Pruefen:
  ./scripts/bridge-service.sh status    laeuft der Dienst?
  ./scripts/bridge-service.sh log       was sieht er?

Rueckgaengig:
  ./scripts/install.sh --uninstall
TXT

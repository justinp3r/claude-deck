#!/usr/bin/env bash
# Uebersetzen und auf das Board schreiben.
#   ./scripts/flash.sh            uebersetzen + flashen
#   ./scripts/flash.sh -m         danach den seriellen Monitor oeffnen
#   ./scripts/flash.sh -c         nur uebersetzen
#   ./scripts/flash.sh -t         mit Selbsttest-Schalter (ALLOW_REMOTE_TAP=1),
#                                 nur zum Pruefen der Kette, nie im Betrieb
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/common.sh

MONITOR=0; COMPILE_ONLY=0; SELFTEST=0
while getopts "mct" o; do case $o in m) MONITOR=1;; c) COMPILE_ONLY=1;; t) SELFTEST=1;; esac; done

EXTRA=()
if [ "$SELFTEST" = "1" ]; then
  # compiler.cpp.extra_flags und NICHT build.extra_flags - dort stehen die
  # USB-Defines des ESP32-Cores, ueberschrieben bleibt das Board stumm.
  EXTRA=(--build-property "compiler.cpp.extra_flags=-DALLOW_REMOTE_TAP=1")
  echo "ACHTUNG: Build mit Selbsttest-Schalter - nicht im Betrieb verwenden."
fi

if [ "$COMPILE_ONLY" = "1" ]; then
  echo "Uebersetzen ..."
  arduino-cli compile --fqbn "$FQBN" ${EXTRA[@]+"${EXTRA[@]}"} "$SKETCH_DIR" 2>&1 \
    | grep -vE "GNU-stack|deprecated and will be removed"
  exit 0
fi

PORT="$(detect_port)" || { echo "Kein Board gefunden. USB-Kabel steckt?"; exit 1; }

# Die Bridge haelt den Port - sonst "Resource busy".
BRIDGE_PLIST="$HOME/Library/LaunchAgents/com.claude-deck.bridge.plist"
BRIDGE_WAS_UP=0
if [ -f "$BRIDGE_PLIST" ] && launchctl list 2>/dev/null | grep -q com.claude-deck.bridge; then
  echo "Bridge anhalten ..."
  launchctl unload "$BRIDGE_PLIST" 2>/dev/null || true
  BRIDGE_WAS_UP=1
  sleep 1
fi
restore_bridge() {
  if [ "$BRIDGE_WAS_UP" = "1" ]; then
    echo "Bridge wieder starten ..."
    launchctl load "$BRIDGE_PLIST" 2>/dev/null || true
  fi
}
trap restore_bridge EXIT
# In einem Aufruf: "upload" allein baut nicht neu und schriebe ein altes Binary.
echo "Uebersetzen und flashen auf $PORT ..."
arduino-cli compile --fqbn "$FQBN" ${EXTRA[@]+"${EXTRA[@]}"} --upload -p "$PORT" "$SKETCH_DIR" 2>&1 \
  | grep -vE "GNU-stack|deprecated and will be removed"
echo "Fertig."

if [ "$MONITOR" = "1" ]; then
  echo "Monitor auf $PORT - beenden mit Strg-C"
  sleep 2
  arduino-cli monitor -p "$PORT" -c baudrate=115200
fi

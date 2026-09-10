#!/usr/bin/env bash
# Einmalige Einrichtung. Laesst sich gefahrlos mehrfach ausführen.
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/common.sh

say() { printf '\n\033[1m%s\033[0m\n' "$*"; }

say "1/5  arduino-cli"
command -v arduino-cli >/dev/null || { echo "fehlt: brew install arduino-cli"; exit 1; }
arduino-cli version

say "2/5  ESP32-Core"
# Der Core steht nicht im Standard-Index, die URL muss zuerst eingetragen werden.
arduino-cli config init --overwrite >/dev/null 2>&1 || true
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json 2>/dev/null || true
arduino-cli core update-index >/dev/null
arduino-cli core install esp32:esp32 2>&1 | tail -1

say "3/5  Bibliotheken"
# LVGL 9.3.0: die Version, gegen die das Waveshare-Beispiel gebaut ist.
arduino-cli lib install lvgl@9.3.0 2>&1 | tail -1
# ArduinoJson 7 ist Pflicht: erst dort heisst der Typ JsonDocument ohne Groesse.
arduino-cli lib install ArduinoJson@7.4.3 2>&1 | tail -1

say "4/5  lv_conf.h"
# LVGL sucht die Datei direkt neben dem lvgl-Ordner.
mkdir -p "$ARDUINO_LIBS"
cp lv_conf.h "$ARDUINO_LIBS/lv_conf.h"
echo "-> $ARDUINO_LIBS/lv_conf.h"

say "5/5  ctags fuer Apple Silicon"
# Arduinos ctags ist ein x86-Binary; ohne Rosetta bricht jeder Build ab.
CTAGS_DIR="$HOME/Library/Arduino15/packages/builtin/tools/ctags/5.8-arduino11"
if [ -d "$CTAGS_DIR" ] && [ "$(uname -m)" = "arm64" ]; then
  if ! file -b "$CTAGS_DIR/ctags" 2>/dev/null | grep -q arm64; then
    command -v ctags >/dev/null 2>&1 || brew install universal-ctags
    [ -e "$CTAGS_DIR/ctags.x86_64.bak" ] || mv "$CTAGS_DIR/ctags" "$CTAGS_DIR/ctags.x86_64.bak"
    ln -sf "$(command -v ctags)" "$CTAGS_DIR/ctags"
    echo "-> natives ctags verlinkt"
  else
    echo "-> schon nativ"
  fi
else
  echo "-> nicht noetig"
fi

say "Fertig. Weiter mit: ./scripts/flash.sh"

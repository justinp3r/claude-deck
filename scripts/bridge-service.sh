#!/usr/bin/env bash
# Bridge als Hintergrunddienst (macOS LaunchAgent), damit sie nach dem
# Anmelden von selbst laeuft.
#
#   ./scripts/bridge-service.sh install     einrichten und starten
#   ./scripts/bridge-service.sh uninstall   entfernen
#   ./scripts/bridge-service.sh status      laeuft sie?
#   ./scripts/bridge-service.sh log         Protokoll mitlesen
set -euo pipefail
cd "$(dirname "$0")/.."
REPO="$(pwd)"
LABEL="com.claude-deck.bridge"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
LOG="$HOME/.claude-deck/bridge.log"
NODE="$(command -v node)"

case "${1:-status}" in
  install)
    mkdir -p "$HOME/Library/LaunchAgents" "$HOME/.claude-deck"
    cat > "$PLIST" <<PLIST_EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>$LABEL</string>
  <key>ProgramArguments</key>
  <array>
    <string>$NODE</string>
    <string>$REPO/bridge/bridge.mjs</string>
  </array>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>StandardOutPath</key><string>$LOG</string>
  <key>StandardErrorPath</key><string>$LOG</string>
</dict>
</plist>
PLIST_EOF
    launchctl unload "$PLIST" 2>/dev/null || true
    launchctl load "$PLIST"
    echo "Eingerichtet. Protokoll: $LOG"
    ;;
  uninstall)
    launchctl unload "$PLIST" 2>/dev/null || true
    rm -f "$PLIST"
    echo "Entfernt."
    ;;
  status)
    if launchctl list | grep -q "$LABEL"; then
      echo "Dienst laeuft:"; launchctl list | grep "$LABEL"
    else
      echo "Dienst laeuft nicht."
    fi
    [ -S "$HOME/.claude-deck/bridge.sock" ] && echo "Socket da." || echo "Kein Socket."
    ;;
  log) tail -f "$LOG" ;;
  *) echo "install | uninstall | status | log"; exit 1 ;;
esac

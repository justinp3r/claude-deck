#!/usr/bin/env bash
# Statusline-Vorschalter: reicht die Limits an die Bridge weiter und ruft danach
# deine urspruengliche Statusline auf. Die Verbrauchsdaten (rate_limits.*) liefert
# Claude Code ausschliesslich hierher - kein CLI-Befehl, kein Hook. Kein
# Node-Prozess pro Render: die Statusline darf nicht langsamer werden.

set -uo pipefail

input=$(cat)
SOCK="$HOME/.claude-deck/bridge.sock"

# Die urspruenglich eingetragene Statusline, abgelegt von settings-patch.mjs.
ORIG_FILE="$HOME/.claude-deck/original-statusline"

# Im Hintergrund, Fehler egal - die Statusline darf daran nie haengen bleiben.
if [ -S "$SOCK" ]; then
  printf '{"op":"usage","payload":%s}\n' "$input" | nc -U "$SOCK" >/dev/null 2>&1 &
fi

if [ -s "$ORIG_FILE" ]; then
  ORIGINAL=$(cat "$ORIG_FILE")
  printf '%s' "$input" | eval "$ORIGINAL"
fi

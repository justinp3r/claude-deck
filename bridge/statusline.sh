#!/usr/bin/env bash
# Statusline-Vorschalter: reicht die Limits an die Bridge weiter und ruft
# danach deine urspruengliche Statusline auf.
#
# Warum ueberhaupt hier: die Verbrauchsdaten (rate_limits.five_hour /
# .seven_day) liefert Claude Code ausschliesslich an die Statusline. Es gibt
# keinen anderen Weg, an sie heranzukommen - kein CLI-Befehl, kein Hook.
#
# Es wird bewusst kein Node-Prozess gestartet: die Statusline rendert oft,
# und nc schreibt die Zeile in den Socket, ohne dass jemand darauf wartet.

set -uo pipefail

input=$(cat)
SOCK="$HOME/.claude-dashboard/bridge.sock"

# Die urspruenglich eingetragene Statusline. settings-patch.mjs legt sie hier
# ab, bevor es sich davorschaltet - so laeuft sie unveraendert weiter und die
# Deinstallation kann sie zurueckschreiben.
ORIG_FILE="$HOME/.claude-dashboard/original-statusline"

# Weiterreichen, im Hintergrund, Fehler egal - die Statusline darf daran
# niemals haengen bleiben.
if [ -S "$SOCK" ]; then
  printf '{"op":"usage","payload":%s}\n' "$input" | nc -U "$SOCK" >/dev/null 2>&1 &
fi

# Die eigentliche Statusline unveraendert weiterlaufen lassen.
if [ -s "$ORIG_FILE" ]; then
  ORIGINAL=$(cat "$ORIG_FILE")
  printf '%s' "$input" | eval "$ORIGINAL"
fi

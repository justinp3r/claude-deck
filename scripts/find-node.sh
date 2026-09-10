#!/usr/bin/env bash
# Gibt den Pfad zu node aus, oder nichts (Exit 1).
#
# Warum das noetig ist: Hooks und LaunchAgents starten ohne dein Shell-Profil.
# Wer node ueber nvm installiert hat - und das ist der Normalfall - hat es unter
# ~/.nvm/versions/node/<version>/bin, und dieser Pfad kommt ausschliesslich
# durch die Profil-Einrichtung in die PATH. Ein blosses "node" schlaegt dort
# mit "env: node: No such file or directory" fehl.

for c in \
  "$(command -v node 2>/dev/null)" \
  /opt/homebrew/bin/node \
  /usr/local/bin/node \
  /usr/bin/node
do
  [ -n "$c" ] && [ -x "$c" ] && { echo "$c"; exit 0; }
done

# nvm: neueste Version gewinnt, damit ein Update nicht alles lahmlegt
for c in $(ls -d "$HOME"/.nvm/versions/node/*/bin/node 2>/dev/null | sort -V -r); do
  [ -x "$c" ] && { echo "$c"; exit 0; }
done

exit 1

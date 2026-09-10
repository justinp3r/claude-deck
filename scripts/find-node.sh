#!/usr/bin/env bash
# Gibt den Pfad zu node aus, oder nichts (Exit 1). Noetig, weil Hooks und
# LaunchAgents ohne Shell-Profil starten und ein nvm-node damit nicht in der PATH ist.

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

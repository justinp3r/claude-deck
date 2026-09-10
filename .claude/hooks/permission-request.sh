#!/usr/bin/env bash
# Startet den PermissionRequest-Hook.
#
# Diese Zwischenschicht existiert nur, um node zu finden: Hooks laufen ohne
# Shell-Profil, und bei einer nvm-Installation liegt node an einem Pfad, den
# nur das Profil kennt. Ohne den Umweg endet der Hook mit Exit 127.
#
# Findet sich kein node, endet das Skript mit Exit 0 und ohne Ausgabe - also
# "keine Entscheidung", und Claude Code fragt wie gewohnt im Terminal. Auch
# dieser Pfad darf nichts freigeben.
set -uo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

NODE="$("$DIR/../../scripts/find-node.sh" 2>/dev/null)" || exit 0
[ -n "$NODE" ] || exit 0

exec "$NODE" "$DIR/permission-request.mjs"

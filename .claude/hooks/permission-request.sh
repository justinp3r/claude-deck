#!/usr/bin/env bash
# Startet den PermissionRequest-Hook. Diese Zwischenschicht sucht nur node: Hooks
# laufen ohne Shell-Profil und finden ein nvm-node sonst nicht (Exit 127).
# Kein node gefunden = Exit 0 ohne Ausgabe = keine Entscheidung, das Terminal fragt.
set -uo pipefail
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

NODE="$("$DIR/../../scripts/find-node.sh" 2>/dev/null)" || exit 0
[ -n "$NODE" ] || exit 0

exec "$NODE" "$DIR/permission-request.mjs"

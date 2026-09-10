#!/usr/bin/env node
/* Traegt Hook und Statusline in ~/.claude/settings.json ein oder wieder aus.
 * Umkehrbar, vor jeder Aenderung wird gesichert. */
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';

const REPO     = path.resolve(new URL('..', import.meta.url).pathname);
const HOME     = os.homedir();
const SETTINGS = process.env.CD_SETTINGS || path.join(HOME, '.claude', 'settings.json');
const RUN_DIR  = path.join(HOME, '.claude-deck');
const ORIG_SL  = path.join(RUN_DIR, 'original-statusline');

/* Ueber den bash-Wrapper: Hooks starten ohne Shell-Profil und finden nvm-node sonst nicht. */
const HOOK_CMD = `bash ${path.join(REPO, '.claude', 'hooks', 'permission-request.sh')}`;
const SL_CMD   = `bash ${path.join(REPO, 'bridge', 'statusline.sh')}`;

const mode = process.argv[2];
if (mode !== 'install' && mode !== 'uninstall') {
  console.error('Aufruf: settings-patch.mjs install|uninstall');
  process.exit(2);
}

fs.mkdirSync(RUN_DIR, { recursive: true });
fs.mkdirSync(path.dirname(SETTINGS), { recursive: true });

let cfg = {};
if (fs.existsSync(SETTINGS)) {
  const raw = fs.readFileSync(SETTINGS, 'utf8');
  try { cfg = JSON.parse(raw); }
  catch { console.error(`${SETTINGS} ist kein gueltiges JSON - bitte erst reparieren.`); process.exit(1); }
  const bak = `${SETTINGS}.bak-${new Date().toISOString().replace(/[:.]/g, '').slice(0, 15)}`;
  fs.writeFileSync(bak, raw);
  console.log(`Sicherung: ${bak}`);
}

const isOurs = c => typeof c === 'string' && c.includes('claude-deck');

if (mode === 'install') {
  /* --- Hook --- */
  cfg.hooks ??= {};
  const list = cfg.hooks.PermissionRequest ?? [];
  /* Vorhandenen eigenen Eintrag aktualisieren, sonst bleibt der alte Aufrufweg stehen. */
  let updated = false;
  for (const g of list) {
    for (const h of g.hooks ?? []) {
      if (isOurs(h.command) && h.command !== HOOK_CMD) { h.command = HOOK_CMD; updated = true; }
      else if (isOurs(h.command)) { updated = true; }
    }
  }
  if (updated) {
    cfg.hooks.PermissionRequest = list;
    console.log('Hook: aktualisiert');
  } else {
    list.push({ hooks: [{ type: 'command', command: HOOK_CMD, timeout: 60,
                          statusMessage: 'Fragt das Panel' }] });
    cfg.hooks.PermissionRequest = list;
    console.log('Hook: eingetragen');
  }

  /* --- Statusline --- */
  const cur = cfg.statusLine?.command;
  if (isOurs(cur)) {
    console.log('Statusline: war schon vorgeschaltet');
  } else {
    /* Bisherigen Befehl merken - der Vorschalter ruft ihn auf, die Deinstallation schreibt ihn zurueck. */
    fs.writeFileSync(ORIG_SL, cur ?? '');
    cfg.statusLine = { type: 'command', command: SL_CMD };
    console.log(cur ? `Statusline: vorgeschaltet (vorher: ${cur})`
                    : 'Statusline: eingerichtet (vorher keine)');
  }
} else {
  /* --- Hook entfernen --- */
  const list = cfg.hooks?.PermissionRequest ?? [];
  const kept = list
    .map(g => ({ ...g, hooks: (g.hooks ?? []).filter(h => !isOurs(h.command)) }))
    .filter(g => g.hooks.length);
  if (kept.length) cfg.hooks.PermissionRequest = kept;
  else if (cfg.hooks) delete cfg.hooks.PermissionRequest;
  if (cfg.hooks && !Object.keys(cfg.hooks).length) delete cfg.hooks;
  console.log('Hook: entfernt');

  /* --- Statusline zuruecksetzen --- */
  if (isOurs(cfg.statusLine?.command)) {
    const prev = fs.existsSync(ORIG_SL) ? fs.readFileSync(ORIG_SL, 'utf8').trim() : '';
    if (prev) { cfg.statusLine = { type: 'command', command: prev };
                console.log(`Statusline: zurueckgesetzt auf ${prev}`); }
    else      { delete cfg.statusLine; console.log('Statusline: entfernt'); }
  }
}

fs.writeFileSync(SETTINGS, JSON.stringify(cfg, null, 2) + '\n');
console.log(`Geschrieben: ${SETTINGS}`);

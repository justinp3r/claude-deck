#!/usr/bin/env node
/* PermissionRequest-Hook: fragt das Geraet statt des Terminals.
 *
 * Die eine Regel, an der alles haengt: JEDER Fehlerpfad gibt die Frage
 * zurueck ans Terminal. Keine Bridge, kein Geraet, Zeit abgelaufen, kaputtes
 * JSON - immer Exit 0 ohne Ausgabe. Laut Hook-Doku laesst das den normalen
 * Berechtigungsablauf unveraendert, der Dialog erscheint also wie sonst.
 * Ein stummes Geraet darf nie ein zustimmendes sein.
 */
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';

const SOCK = path.join(os.homedir(), '.claude-dashboard', 'bridge.sock');
const TTL  = Math.min(Math.max(Number(process.env.CLAUDE_DASHBOARD_TTL) || 30, 5), 300);

/* Keine Ausgabe = keine Entscheidung = Terminal fragt. */
function fallback() { process.exit(0); }

function answer(behavior, message) {
  const decision = { behavior };
  if (behavior === 'deny') decision.message = message || 'Denied on the panel';
  process.stdout.write(JSON.stringify({
    hookSpecificOutput: {
      hookEventName: 'PermissionRequest',
      decision,
    },
  }));
  process.exit(0);
}

let raw = '';
process.stdin.setEncoding('utf8');
process.stdin.on('data', d => { raw += d; });
process.stdin.on('error', fallback);
process.stdin.on('end', () => {
  let payload;
  try { payload = JSON.parse(raw); } catch { return fallback(); }

  const sock = net.createConnection(SOCK);
  let done = false;
  const finish = fn => { if (!done) { done = true; try { sock.destroy(); } catch {} fn(); } };

  /* Etwas Luft ueber die Geraete-Frist, damit die Bridge zuerst aufgibt. */
  const timer = setTimeout(() => finish(fallback), (TTL + 3) * 1000);
  timer.unref?.();

  sock.on('error', () => finish(fallback));
  sock.on('close', () => finish(fallback));

  sock.on('connect', () => {
    sock.write(JSON.stringify({ op: 'request', ttl: TTL, payload }) + '\n');
  });

  let buf = '';
  sock.on('data', d => {
    buf += d.toString('utf8');
    const i = buf.indexOf('\n');
    if (i < 0) return;
    let msg;
    try { msg = JSON.parse(buf.slice(0, i)); } catch { return finish(fallback); }
    clearTimeout(timer);
    if (msg.decision === 'allow')      finish(() => answer('allow'));
    else if (msg.decision === 'deny')  finish(() => answer('deny'));
    else                               finish(fallback);
  });
});

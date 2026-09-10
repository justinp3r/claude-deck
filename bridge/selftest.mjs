#!/usr/bin/env node
/* Prueft die Kette Hook -> Bridge -> Geraet -> Hook.
 *
 * Setzt eine laufende Bridge und eine Firmware mit ALLOW_REMOTE_TAP=1 voraus:
 *   arduino-cli compile --build-property compiler.cpp.extra_flags=-DALLOW_REMOTE_TAP=1 ...
 */
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import { spawn } from 'node:child_process';

const SOCK = path.join(os.homedir(), '.claude-deck', 'bridge.sock');
const HOOK = new URL('../.claude/hooks/permission-request.mjs', import.meta.url).pathname;

const payload = {
  session_id: 'selftest', cwd: process.cwd(),
  permission_mode: 'default', hook_event_name: 'PermissionRequest',
  tool_name: 'Bash',
  tool_input: { command: 'git push origin main --force', description: 'Force push' },
};

function tap(v) {
  return new Promise(res => {
    const s = net.createConnection(SOCK, () => {
      s.write(JSON.stringify({ op: 'tap', v }) + '\n');
      s.end(); res();
    });
    s.on('error', res);
  });
}

function runHook(ttl) {
  return new Promise(res => {
    const p = spawn('node', [HOOK], { env: { ...process.env, CLAUDE_DECK_TTL: String(ttl) } });
    let out = '';
    p.stdout.on('data', d => out += d);
    p.on('close', code => res({ code, out }));
    p.stdin.end(JSON.stringify(payload));
  });
}

async function check(name, v, expect) {
  const run = runHook(15);
  await new Promise(r => setTimeout(r, 2500));
  if (v) await tap(v);
  const { code, out } = await run;
  let got = 'fallback';
  try { got = JSON.parse(out).hookSpecificOutput.decision.behavior; } catch {}
  const ok = code === 0 && got === expect;
  console.log(`${ok ? 'ok  ' : 'FEHLER'}  ${name}: exit=${code} -> ${got} (erwartet ${expect})`);
  return ok;
}

const results = [];
results.push(await check('Accept am Geraet', 'allow', 'allow'));
results.push(await check('Deny am Geraet', 'deny', 'deny'));
console.log('\nZeitablauf ohne Eingabe (muss ins Terminal zurueckfallen, dauert ~8 s) ...');
const r = await runHook(6);
let got = 'fallback';
try { got = JSON.parse(r.out).hookSpecificOutput.decision.behavior; } catch {}
const ok = r.code === 0 && r.out.trim() === '';
console.log(`${ok ? 'ok  ' : 'FEHLER'}  Zeitablauf: exit=${r.code}, Ausgabe leer=${r.out.trim() === ''}`);
results.push(ok);

console.log(results.every(Boolean) ? '\nAlles gruen.' : '\nMindestens ein Test rot.');
process.exit(results.every(Boolean) ? 0 : 1);

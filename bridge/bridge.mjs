#!/usr/bin/env node
/* claude-dashboard Bridge.
 *
 * Haelt den seriellen Port zum Geraet und einen Unix-Socket fuer die Hooks.
 * Warum ein Daemon dazwischen und nicht der Hook direkt am Port: den Port kann
 * immer nur ein Prozess offen haben, und es laufen typischerweise mehrere
 * Claude-Code-Sessions gleichzeitig. Der Daemon serialisiert das und kennt
 * dadurch auch die Warteschlange.
 *
 * Ohne npm-Abhaengigkeiten: der serielle Port wird mit stty konfiguriert und
 * danach als normale Datei geoeffnet.
 */
import fs from 'node:fs';
import net from 'node:net';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';

const HOME      = os.homedir();
const RUN_DIR   = path.join(HOME, '.claude-dashboard');
const SOCK      = path.join(RUN_DIR, 'bridge.sock');
const BAUD      = '115200';
const PING_MS   = 4000;
const DEAD_MS   = 12000;   /* ohne Lebenszeichen gilt das Geraet als weg */

const log = (...a) => console.error(new Date().toISOString().slice(11, 19), ...a);

/* ------------------------------------------------------------------ */
/* Serieller Port                                                      */
/* ------------------------------------------------------------------ */

function findPort() {
  /* Uebersteuerung zum Testen: CLAUDE_DASHBOARD_PORT=/dev/null-ish simuliert
   * ein gezogenes Kabel, ohne dass jemand daran ziehen muss. */
  const forced = process.env.CLAUDE_DASHBOARD_PORT;
  if (forced) return fs.existsSync(forced) ? forced : null;
  const dev = fs.readdirSync('/dev');
  const hit = dev.find(n => n.startsWith('cu.usbmodem'))
           || dev.find(n => n.startsWith('cu.wchusbserial'))
           || dev.find(n => n.startsWith('cu.SLAB_USBtoUART'));
  return hit ? `/dev/${hit}` : null;
}

/* Lesen per nicht-blockierendem readSync im Intervall statt per Stream:
 * fs.createReadStream auf einem TTY verhaelt sich in Node unzuverlaessig und
 * kann den Event-Loop blockieren. Ein Zeichengeraet mit O_NONBLOCK abzufragen
 * ist unspektakulaer, aber vorhersagbar. */
class Device {
  constructor(onLine) {
    this.onLine = onLine;
    this.fd = null;
    this.rbuf = Buffer.alloc(8192);
    this.buf = '';
    this.lastSeen = 0;
    this.path = null;
    this.poll = null;
  }

  get alive() { return this.fd !== null && (Date.now() - this.lastSeen) < DEAD_MS; }

  open() {
    if (this.fd !== null) return true;
    const p = findPort();
    if (!p) return false;
    try {
      spawnSync('stty', ['-f', p, BAUD, 'raw', '-echo']);
      this.fd = fs.openSync(p, fs.constants.O_RDWR | fs.constants.O_NONBLOCK);
      this.path = p;
      this.lastSeen = Date.now();
      this.poll = setInterval(() => this.drain(), 20);
      this.poll.unref?.();
      log('Geraet offen:', p);
      return true;
    } catch (e) {
      log('Port laesst sich nicht oeffnen:', e.message);
      this.close();
      return false;
    }
  }

  close() {
    if (this.poll) { clearInterval(this.poll); this.poll = null; }
    if (this.fd !== null) { try { fs.closeSync(this.fd); } catch {} ; log('Geraet zu'); }
    this.fd = null; this.buf = '';
  }

  drain() {
    if (this.fd === null) return;
    for (;;) {
      let n = 0;
      try { n = fs.readSync(this.fd, this.rbuf, 0, this.rbuf.length, null); }
      catch (e) {
        if (e.code === 'EAGAIN') return;
        if (e.code === 'ENXIO' || e.code === 'EBADF' || e.code === 'EIO') { this.close(); }
        return;
      }
      if (n <= 0) return;
      this.feed(this.rbuf.subarray(0, n));
    }
  }

  feed(chunk) {
    this.buf += chunk.toString('utf8');
    let i;
    while ((i = this.buf.indexOf('\n')) >= 0) {
      const line = this.buf.slice(0, i).trim();
      this.buf = this.buf.slice(i + 1);
      if (!line.startsWith('{')) continue;      /* Bootlog ignorieren */
      this.lastSeen = Date.now();
      try { this.onLine(JSON.parse(line)); } catch {}
    }
    if (this.buf.length > 8192) this.buf = '';
  }

  send(obj) {
    if (this.fd === null) return false;
    try {
      fs.writeSync(this.fd, JSON.stringify(obj) + '\n');
      return true;
    } catch (e) {
      if (e.code === 'EAGAIN') return true;
      log('Schreiben fehlgeschlagen:', e.code || e.message);
      this.close();
      return false;
    }
  }
}

/* ------------------------------------------------------------------ */
/* Anfrage lesbar machen                                               */
/* ------------------------------------------------------------------ */

/* Breite der Kontextspalte in Zeichen: 272 px bei IBM Plex Mono 21 px
 * (0,6 em Vorschub) sind rund 21 Zeichen. Im Warteschlangen-Zustand ist die
 * Spalte etwas schmaler, deshalb 20. */
const COLS = 20;

function splitTwo(text) {
  const t = (text || '').replace(/\s+/g, ' ').trim();
  if (t.length <= COLS) return [t, ''];
  /* An einer Wortgrenze trennen, wenn eine in der Naehe liegt. */
  let cut = t.lastIndexOf(' ', COLS);
  if (cut < COLS * 0.5) cut = COLS;
  return [t.slice(0, cut).trim(), t.slice(cut).trim().slice(0, COLS * 2)];
}

function shorten(p) {
  if (!p) return '';
  return p.startsWith(HOME) ? '~' + p.slice(HOME.length) : p;
}

function describe(h) {
  const tool = h.tool_name || 'Tool';
  const ti   = h.tool_input || {};
  const cwd  = shorten(h.cwd || '');
  const project = path.basename(h.cwd || 'session') || 'session';

  let full = '';
  let note = '';
  if (tool === 'Bash' || tool === 'PowerShell') {
    full = ti.command || '';
    note = ti.description || '';
  } else if (ti.file_path) {
    full = shorten(ti.file_path);
    note = ti.description || '';
  } else if (ti.url) {
    full = ti.url;
  } else {
    full = Object.entries(ti)
      .filter(([, v]) => typeof v === 'string' || typeof v === 'number')
      .map(([k, v]) => `${k}=${v}`).join(' ');
  }
  full = String(full).replace(/\s+/g, ' ').trim();

  const [l1, l2] = splitTwo(full);
  const initials = project.replace(/[^a-zA-Z0-9]/g, '').slice(0, 2).toLowerCase() || '??';

  return {
    agent: project, init: initials, tool,
    l1, l2, full, cwd: cwd ? `cwd ${cwd}` : '',
    note, warn: '', dhdr: `${tool} · ${project}`,
  };
}

/* ------------------------------------------------------------------ */
/* Bridge                                                              */
/* ------------------------------------------------------------------ */

const pending  = new Map();   /* id -> entry */
const order    = [];          /* Reihenfolge der offenen Anfragen */
const sessions = new Map();   /* session_id -> { name, last } */
let seq = 0;
let usage = null;             /* zuletzt von der Statusline gemeldete Limits */
let lastIdleSig = '';         /* zuletzt gesendeter Ruhezustand, gegen Wiederholungen */

const dev = new Device(onDeviceLine);

function clock() {
  const d = new Date();
  return `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
}

/* "2h 14m" / "4d 6h" / "now". resets_at kommt als Unix-Sekunden. */
function until(epoch) {
  if (!epoch) return '';
  let s = Math.round(epoch - Date.now() / 1000);
  if (s <= 0) return 'now';
  const d = Math.floor(s / 86400); s -= d * 86400;
  const h = Math.floor(s / 3600);  s -= h * 3600;
  const m = Math.floor(s / 60);
  if (d) return `${d}d ${h}h left`;
  if (h) return `${h}h ${m}m left`;
  return `${m}m left`;
}

function usagePayload() {
  const r = usage?.rate_limits;
  const five = r?.five_hour, week = r?.seven_day;
  /* Nur die kontoweiten Fenster. context_window gehoert einer einzelnen
   * Session und wuerde auf einer geraeteweiten Uebersicht nur springen. */
  return {
    t: 'usage',
    have: !!(five || week),
    h5:  five ? Math.round(five.used_percentage) : -1,
    h5r: five ? until(five.resets_at) : '',
    wk:  week ? Math.round(week.used_percentage) : -1,
    wkr: week ? until(week.resets_at) : '',
  };
}

function idlePayload() {
  const now = Date.now();
  for (const [k, v] of sessions) if (now - v.last > 30 * 60_000) sessions.delete(k);

  /* Das Geraet zeigt drei auf einmal und blaettert durch den Rest. */
  const top = [...sessions.entries()]
    .sort((a, b) => b[1].last - a[1].last)
    .slice(0, 9);

  /* Der Anzeigename kommt aus dem Verzeichnis, und zwei Sessions im selben
   * Ordner sind voellig normal. Nur dann - und nur dann - haengen wir ein
   * Stueck der Session-ID an, damit die Zeilen unterscheidbar bleiben. */
  const seen = new Map();
  for (const [, v] of top) seen.set(v.name, (seen.get(v.name) || 0) + 1);

  const rows = top.map(([id, s]) => {
    const min = Math.floor((now - s.last) / 60_000);
    const name = seen.get(s.name) > 1 ? `${s.name} ${id.slice(-4)}` : s.name;
    return { n: name, a: min < 1 ? 'now' : `${min} m`, on: (now - s.last) < 120_000 };
  });

  const n = sessions.size;
  return {
    t: 'idle',
    hdr: n === 0 ? 'no sessions' : n === 1 ? '1 session' : `${n} sessions`,
    clock: clock(),
    rows,
  };
}

function pushState() {
  if (!dev.alive) return;
  dev.send(usagePayload());
  const head = order[0] ? pending.get(order[0]) : null;
  if (!head) { dev.send(idlePayload()); return; }

  const nextId = order[1];
  dev.send({
    t: 'req', id: head.id, ...head.view,
    qpos: 1, qtot: order.length,
    next: nextId ? pending.get(nextId).view.agent : '',
    ttl: Math.max(1, Math.round((head.deadline - Date.now()) / 1000)),
  });
}

function settle(id, value) {
  const e = pending.get(id);
  if (!e) return;
  pending.delete(id);
  const i = order.indexOf(id);
  if (i >= 0) order.splice(i, 1);
  clearTimeout(e.timer);
  try {
    e.socket.write(JSON.stringify({ decision: value }) + '\n');
    e.socket.end();
  } catch {}
  pushState();
}

function onDeviceLine(msg) {
  if (msg.t === 'focus') {
    /* Wischen am Geraet blaettert durch die offenen Anfragen. Die Bridge
     * bleibt die Wahrheit darueber, was vorne liegt. */
    if (order.length > 1) {
      const d = msg.d > 0 ? 1 : -1;
      if (d > 0) order.push(order.shift());
      else       order.unshift(order.pop());
      pushState();
    }
    return;
  }
  if (msg.t === 'decision') {
    log('Geraet:', msg.v, msg.id);
    settle(msg.id, msg.v === 'allow' ? 'allow' : 'deny');
  } else if (msg.t === 'hello') {
    log('Geraet gestartet');
    pushState();
  }
}

/* ------------------------------------------------------------------ */
/* Socket fuer die Hooks                                               */
/* ------------------------------------------------------------------ */

/* Eine gesehene Session merken. Quelle ist entweder ein Hook-Aufruf oder die
 * Statusline - beide liefern dieselben Felder. */
function noteSession(p) {
  if (!p || !p.session_id) return;
  const dir = p.cwd || p.workspace?.current_dir || '';
  const name = p.session_name || path.basename(dir) || 'session';
  if (!sessions.has(p.session_id)) log('Session gesehen:', name, `(${p.session_id.slice(-6)})`);
  sessions.set(p.session_id, { name, last: Date.now() });
  return name;
}

/* Ruhezustand senden, aber nur bei echter Aenderung. */
function pushIdle() {
  const p = idlePayload();
  const sig = JSON.stringify(p);
  if (sig === lastIdleSig) return;
  lastIdleSig = sig;
  dev.send(p);
}

function onHook(socket, req) {
  /* Testkanal fuer scripts/selftest.mjs: simuliert einen Tastendruck am Geraet.
   * Wirkt nur, wenn die Firmware mit ALLOW_REMOTE_TAP=1 gebaut ist - im
   * Normalbetrieb verwirft das Geraet das unbekannte Kommando. */
  if (req.op === 'usage') {
    const before = usage ? JSON.stringify(usage.rate_limits || {}) : '';
    usage = req.payload || null;
    const after = usage ? JSON.stringify(usage.rate_limits || {}) : '';
    if (after !== before) log('Limits:', after || '(keine rate_limits im Payload)');

    /* Die Statusline laeuft in jeder aktiven Session, dauernd - und liefert
     * session_id und cwd mit. Damit kennt die Bridge eine Session ab dem
     * ersten Rendern, nicht erst bei der ersten Freigabeanfrage. Genau das
     * macht die Ruheansicht ueberhaupt nuetzlich. */
    noteSession(usage);

    if (dev.alive) {
      dev.send(usagePayload());
      if (order.length === 0) pushIdle();
    }
    socket.end();
    return;
  }

  if (req.op === 'tap') {
    dev.send({ t: 'tap', v: req.v === 'allow' ? 'allow' : 'deny' });
    socket.end();
    return;
  }

  const h = req.payload || {};

  noteSession(h);

  /* Kein Geraet, keine Entscheidung: der Hook faellt auf das Terminal zurueck. */
  if (!dev.alive) {
    socket.write(JSON.stringify({ decision: null, reason: 'no device' }) + '\n');
    socket.end();
    return;
  }

  const id  = `r${++seq}`;
  const ttl = Math.min(Math.max(req.ttl || 30, 5), 300);
  const e = {
    id, socket,
    view: describe(h),
    deadline: Date.now() + ttl * 1000,
    timer: setTimeout(() => {
      log('Zeit abgelaufen:', id);
      settle(id, null);            /* null = keine Entscheidung -> Terminal */
    }, ttl * 1000),
  };

  pending.set(id, e);
  order.push(id);
  log('Anfrage', id, e.view.tool, JSON.stringify(e.view.full).slice(0, 60));

  /* Legt der Hook auf (Nutzer hat im Terminal entschieden), Eintrag raeumen. */
  socket.on('close', () => {
    if (pending.has(id)) {
      clearTimeout(e.timer);
      pending.delete(id);
      const i = order.indexOf(id);
      if (i >= 0) order.splice(i, 1);
      dev.send({ t: 'cancel', id });
      pushState();
    }
  });
  socket.on('error', () => {});

  pushState();
}

fs.mkdirSync(RUN_DIR, { recursive: true });
try { fs.unlinkSync(SOCK); } catch {}

const server = net.createServer(socket => {
  let buf = '';
  socket.on('data', d => {
    buf += d.toString('utf8');
    const i = buf.indexOf('\n');
    if (i < 0) return;
    const line = buf.slice(0, i);
    buf = '';
    try { onHook(socket, JSON.parse(line)); }
    catch { try { socket.end(); } catch {} }
  });
  socket.on('error', () => {});
});

server.listen(SOCK, () => {
  fs.chmodSync(SOCK, 0o600);          /* nur der eigene Nutzer */
  log('Bridge laeuft auf', SOCK);
});

/* Port suchen, Geraet wachhalten, Uhr im Ruhezustand aktualisieren. */
let wasAlive = false;
setInterval(() => {
  if (dev.fd === null) dev.open();
  if (dev.fd !== null) dev.send({ t: 'ping' });

  const alive = dev.alive;
  if (alive !== wasAlive) {
    log(alive ? 'Geraet erreichbar' : 'Geraet weg');
    wasAlive = alive;
    if (!alive) {
      dev.close();
      /* Offene Anfragen ans Terminal zurueckgeben. */
      for (const id of [...order]) settle(id, null);
    }
  }
  /* Nur senden, wenn sich wirklich etwas geaendert hat. Spart Funkverkehr und
   * vermeidet, dass das Geraet staendig neu gezeichnet wird. */
  if (alive && order.length === 0) pushIdle();
}, PING_MS);

function shutdown() {
  log('Bridge beendet');
  for (const id of [...order]) settle(id, null);
  if (dev.fd !== null) { dev.send({ t: 'link', up: false }); }
  dev.close();
  try { server.close(); } catch {}
  try { fs.unlinkSync(SOCK); } catch {}
  process.exit(0);
}
process.on('SIGINT', shutdown);
process.on('SIGTERM', shutdown);

dev.open();
pushState();

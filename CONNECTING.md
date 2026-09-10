# Connecting to Claude Code

## How it fits together

```
Claude Code                 Bridge (Node)              ESP32-S3
    │                            │                        │
    │ PermissionRequest hook     │                        │
    ├───────────────────────────►│  USB cable             │
    │  (Unix socket)             ├───────────────────────►│  shows the request
    │                            │                        │
    │◄───────────────────────────┤◄───────────────────────┤  Accept / Deny
    │  allow / deny              │                        │
```

Three parts: a **hook** that Claude Code calls, a **daemon** that holds the serial
port, and the **firmware**. The daemon sits in between because only one process
can hold the port open — and you usually have several Claude Code sessions running
at once. That is also how it knows the queue.

**The hook is `PermissionRequest`, not `PreToolUse`.** That is the difference
between usable and unusable: `PreToolUse` fires before *every* tool call, so also
on every `Read` and `Grep`, and you would have to reimplement the entire
permission logic. `PermissionRequest` fires exactly when the dialog would appear
in the terminal.

## Do you have to install anything?

**To use it: no.** The bridge and the hook are plain Node without a single npm
dependency. You already have Node (v24). The serial port is configured with `stty`
and then read as an ordinary file.

**To change the firmware:** arduino-cli, ESP32 core, LVGL, ArduinoJson — that is
what `./scripts/setup.sh` takes care of.

## Setting up on a new Mac

```bash
git clone <this-repo> claude-deck
cd claude-deck
./scripts/install.sh
```

Then **restart Claude Code once**. That is all.

The script does three things, all reversible:

| | |
|---|---|
| bridge as a service | starts at login from now on |
| `PermissionRequest` hook | registers itself in `~/.claude/settings.json` |
| statusline shim | fetches the limits, then calls your previous statusline unchanged |

Before every change it backs up `settings.json`, and your previous statusline is
remembered, not overwritten. To undo:

```bash
./scripts/install.sh --uninstall
```

**What you do NOT need:** arduino-cli, the Arduino IDE, or anything on the device.
The firmware stays on it. The device is stateless with respect to the computer —
moving the cable over is enough.

The new Mac only needs Node (`brew install node`). The bridge and the hook have
no npm dependencies at all.

## What actually keeps running?

Exactly **one** process: the bridge, started by launchd. It holds the serial port
and the socket.

The hook and the statusline shim are **not** processes. Claude Code starts them
per invocation, they do their work and exit — a few milliseconds. There is nothing
that could get stuck in the background.

```bash
./scripts/install.sh --uninstall   # service gone, hook removed, statusline restored
./scripts/install.sh --purge       # also delete ~/.claude-deck/
```

Verified after `--uninstall`: no process, LaunchAgent unloaded and plist deleted,
socket gone, hook removed, statusline reset to exactly the previous command. All
that stays behind is `bridge.log` and the remembered original statusline in
`~/.claude-deck/` — data, nothing running. `--purge` clears those too.

Afterwards, restart Claude Code once.

## When nothing arrives at the device

Two causes are likely:

**Nothing is being asked at all.** `PermissionRequest` only fires when the dialog
would appear in the terminal. If the action is already covered by your allow rules
or the session runs in bypass mode, there is nothing to confirm.

**The hook cannot find `node`.** Hooks start without your shell profile. With an
nvm installation, `node` lives at a path only the profile knows about — the hook
ends with exit 127 and Claude Code silently keeps asking in the terminal. That is
why a bash wrapper is registered that looks for `node` itself. To check:

```bash
./scripts/find-node.sh          # must print a path
./scripts/bridge-service.sh log # shows incoming requests
```

## How is the device attached? And where do you sign in?

**Over USB-C. No Bluetooth, no Wi-Fi, no pairing.** The same cable that powers the
device is the data link.

**The device has no Claude account and does not need one.** It never talks to
Anthropic. What is signed in is your Claude Code on the Mac, same as before. The
sequence is:

1. Claude Code wants to use a tool and needs your permission
2. The hook passes the question to the bridge on the Mac
3. The bridge sends it over the cable to the device
4. You tap Accept or Deny
5. The answer travels back the same way

There are no credentials on the device, it is not on any network, and it cannot do
anything except display and send `allow`/`deny` back.

## Bluetooth?

**Short answer: no, use the cable.** The longer one:

| | USB (today) | BLE | Wi-Fi |
|---|---|---|---|
| Effort on the Mac | none | `noble`, native dependencies, permission dialogs | none |
| Multiple computers | move the cable | pair again | possible simultaneously |
| Power | comes over the cable | needs a power supply | needs a power supply |
| Latency | ~1 ms | 30–100 ms | ~5 ms on the LAN |
| Who may approve? | whoever is on the cable | whoever is paired | **anyone on the network** |

The ESP32-S3 only does BLE, not Bluetooth Classic. BLE from Node on macOS means
`@abandonware/noble` — native compilation, permission dialogs, and it pairs with
exactly one computer. So for "the other laptop" it does not solve the problem any
better than a cable, while adding a new source of failure.

**Wi-Fi would be the better wireless route** if you ever want to put the device
somewhere on the desk on its own: the S3 has Wi-Fi, the device would be on the
network, and the bridge would talk over TCP. But then you absolutely need a shared
secret — otherwise anyone on the same network can grant your approvals. That is
the actual work involved, not the radio link.

## Trying it out

```bash
./scripts/bridge-service.sh log      # follow along in one terminal
```

Then, in another terminal, start Claude Code in this project and do something that
prompts. The request appears on the device, and the log says
`Anfrage r1 Bash "..."`.

A full self-test without touching the display:

```bash
./scripts/flash.sh -t                # firmware with the self-test switch
node bridge/selftest.mjs             # check Accept, Deny and timeout
./scripts/flash.sh                   # afterwards, without the switch again!
```

## When nothing happens

The hook is built to **do nothing when in doubt** — then the terminal simply asks
as before. That is intentional, not a bug. To find out what is going on:

| Check | Command |
|---|---|
| Is the bridge running? | `./scripts/bridge-service.sh status` |
| What does it say? | `./scripts/bridge-service.sh log` |
| Does the device answer? | the log says `Geraet erreichbar` |
| Does the hook fire? | `claude --debug`, the hook call shows up there |

Most common case: Claude Code is not asking at all, because the action is already
allowed by an allow rule or by the permission mode. Then `PermissionRequest` does
not fire — which is correct.

## Security

The socket lives at `~/.claude-deck/bridge.sock` with mode `0600`; only your
user can reach it.

The self-test switch `ALLOW_REMOTE_TAP` in the firmware is **off** by default and
has to stay that way. If it were on, the computer could approve its own requests —
and that is exactly what the device is there to prevent.

**Every failure path ends in the terminal.** No bridge, no device, cable pulled,
deadline passed, broken JSON: the hook returns no decision and the usual dialog
appears. There is no path on which a silent device approves anything.

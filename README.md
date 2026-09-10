# claude-deck

A physical approval panel for Claude Code. An ESP32-S3 with a wide touch LCD sits
next to the keyboard and answers exactly one question:

> **Is this agent allowed to do that, right now?**

Instead of dismissing every prompt in the terminal, it shows up on the device. One
tap on **Accept** or **Deny** decides what Claude Code does next.

![Approval request: the Clawd tile on the left, the command in the middle, a filled Accept button above an outlined Deny button on the right](docs/img/panel-approval.svg)

![Idle view: three running sessions with status dots and how long each has been going](docs/img/panel-idle.svg)

640 × 172 px on 84 × 22.6 mm. Both buttons are 22 × 8.7 mm — the point below
which you stop hitting them reliably.

<sub>Drawn from the firmware's own layout: coordinates, sizes and colours come
straight out of `ui_theme.h`, `ui_panel.c` and `clawd.c` via
[`docs/render-panels.py`](docs/render-panels.py). Not photographs.</sub>

## Status

Working. The chain Claude Code → hook → bridge → device → decision is proven,
including every failure path.

## Getting started

```bash
git clone <repo> claude-deck && cd claude-deck
./scripts/install.sh
```

Then restart Claude Code once. That is all — assuming the firmware is already on
the device and Node is installed.

To undo: `./scripts/install.sh --uninstall`

**Firmware on a brand-new board**, or after you change something on the device:

```bash
brew install arduino-cli            # once per machine
./scripts/setup.sh                  # ESP32 core, LVGL, ArduinoJson, lv_conf.h, ctags
./scripts/flash.sh                  # compile and flash
```

A board out of the box needs nothing erased and no button held — `flash.sh` writes
the bootloader, partition table and app over the factory demo in one go. The whole
sequence, including what to check when the port does not show up, is in
[FLASHING.md](FLASHING.md).

## How it fits together

```
Claude Code                 Bridge (Node)              ESP32-S3
    │                            │                        │
    │ PermissionRequest hook     │                        │
    ├───────────────────────────►│  USB-C                 │
    │  (Unix socket)             ├───────────────────────►│  shows the request
    │                            │                        │
    │◄───────────────────────────┤◄───────────────────────┤  Accept / Deny
    │  allow / deny              │                        │
```

The daemon sits in between because only one process can hold the serial port open
— and typically several Claude Code sessions are running. That is also how it
knows the queue.

**It is USB-C, not Bluetooth.** The same cable that powers the device is the data
link. The device has **no Claude account** and never talks to Anthropic — it
displays what the bridge sends and sends `allow`/`deny` back. No credentials on
it, nothing over the network.

## The rule everything hangs on

**Every failure path hands the question back to the terminal.** Cable unplugged,
bridge dead, timeout, broken JSON: the hook returns no decision, and the usual
dialog appears just as before. Measured:

| Case | Time until the terminal asks |
|---|---|
| Cable unplugged | 0.06 s |
| Bridge stopped | 0.06 s |
| Device present, nobody taps | 30 s (configurable) |

There is no path on which a silent device approves anything. Not even with a
single noisy sample — a touch only counts once two consecutive readings confirm
it.

## What the device shows

| State | Content |
|---|---|
| `idle` | running sessions; when nothing is going on, Clawd walks across the screen |
| `approval` | tile, command, Accept and Deny |
| `queue` | as above, plus `1 of 2 waiting` and a position bar |
| `detail` | the whole command across the full width |
| `usage` | 5-hour window and week as bars |
| `disconnected` | red bar, `Bridge unreachable` |

![Usage view: a 5-hour bar at 82% in orange and a weekly bar at 38% in blue, each with its percentage and time until reset](docs/img/panel-usage.svg)

The usage bars follow the thresholds at which Claude Code itself warns:
blue below 75%, orange from 75%, red from 95%.

It is operated without visible controls — at 22.6 mm of height, every button is
space taken away from the content:

| Gesture | Effect |
|---|---|
| swipe ↓ | usage view (in `idle`, page back through the session list first) |
| swipe ↑ | back |
| swipe ← → | page through the waiting requests |
| tap the tile | detail view; tap again to go back |

## Hardware

**Waveshare ESP32-S3-Touch-LCD-3.49, revision V2** — 3.49" IPS, 172 × 640 native
(driven in landscape here), AXS15231B over QSPI, touch over I²C, 16 MB flash, 8 MB
octal PSRAM.

Two settings are not optional, or the device will not boot: `PSRAM=opi` (not
`enabled`) and the **V2** pinout. The reasons are in
[CLAUDE.md](CLAUDE.md#hardware).

## Layout

```
firmware/claude_deck/     Arduino sketch: LVGL, UI, protocol
bridge/                   Node daemon, hook glue, self-test
.claude/                  hook registration for this project
scripts/                  install.sh, setup.sh, flash.sh, bridge-service.sh
lv_conf.h                 LVGL configuration
docs/                     the panel images and the script that draws them
reference/                material this was built from, not part of the build
```

Neither the bridge nor the hook has npm dependencies.

## Further reading

| | |
|---|---|
| [CONNECTING.md](CONNECTING.md) | setup, second machine, why not Bluetooth |
| [FLASHING.md](FLASHING.md) | building the firmware, trying it out, failure modes |
| [CLAUDE.md](CLAUDE.md) | architecture, design decisions, every pitfall |

## Self-test

```bash
./scripts/flash.sh -t          # firmware with the test switch
node bridge/selftest.mjs       # check Accept, Deny and timeout
./scripts/flash.sh             # afterwards, without the switch again
```

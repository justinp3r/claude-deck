# Flashing

Everything is already set up and **the firmware is already running on your board**.
You need this guide if you changed something, are setting up a second machine, or
have a board straight out of the box.

## Short version

```bash
cd ~/Dev/claude-deck
./scripts/flash.sh
```

That is it. Compile, find the board, flash.

## From scratch: new machine, brand-new board

A board out of the box carries Waveshare's factory demo. Nothing has to be erased
first — `flash.sh` writes the bootloader, the partition table and the app in one
go, and that replaces the factory image completely.

**1. Toolchain** (once per machine — `arduino-cli` and Node are the only things
Homebrew has to provide):

```bash
brew install arduino-cli node
git clone <this-repo> claude-deck
cd claude-deck
./scripts/setup.sh
```

`setup.sh` installs the ESP32 core, LVGL 9.3.0 and ArduinoJson 7, copies
`lv_conf.h` next to the LVGL library, and swaps Arduino's x86 `ctags` for a native
one on Apple Silicon. It is safe to run again at any time.

**2. Plug the board in.** USB-C, and it has to be a data cable — a charge-only one
gives you a lit board and no port. Check:

```bash
ls /dev/cu.usbmodem*
```

Something has to show up (`/dev/cu.usbmodem101` here). The S3's USB-Serial/JTAG
lives in ROM, so the port appears no matter what firmware is on the chip. If it
stays empty, see *No board found* below.

**3. Flash:**

```bash
./scripts/flash.sh
```

The board is not put into a boot mode by hand and no button is pressed —
arduino-cli resets it over USB by itself. The first build takes a while because
the whole core is compiled; every build after that only redoes what changed
(around 20 s here).

**4. What you should see:** the demo below. It runs until the bridge checks in for
the first time, so a freshly flashed board that is not connected to anything cycles
through its states on its own — that is the sign that it worked.

Connecting the device to Claude Code is a separate step, and one that touches the
board no further: [CONNECTING.md](CONNECTING.md).

### If you want a truly empty chip

Not necessary for flashing, and worth doing only when a board misbehaves in a way
that suggests leftovers in NVS from an earlier firmware:

```bash
~/Library/Arduino15/packages/esp32/tools/esptool_py/*/esptool \
  --chip esp32s3 -p /dev/cu.usbmodem101 erase-flash
./scripts/flash.sh
```

After an erase the board is blank and stays dark until `flash.sh` has run.

## What you should see afterwards

The display runs a demo through all six states — one step every 4 seconds. All
text on the device is English:

| | State | What you see |
|---|---|---|
| 1 | **idle** | up to three sessions; if nothing is running, Clawd walks across the screen |
| 2 | **approval** | tile with Clawd, command, **Accept** filled on top, **Deny** outlined below |
| 3 | **queue** | two bars on the left, `1 of 2 waiting` in amber |
| 4 | **detail** | full command across the full width, orange warning at the bottom |
| 5 | **usage** | two bars: 5-hour window and week, coloured from 75% / 95% |
| 6 | **disconnected** | red bar on the left, `Bridge unreachable` |

Both buttons are in Anthropic orange `#D97757`: **Accept** filled with black text,
**Deny** outlined only, with white text.

## Trying it out

With the serial monitor you can hold individual states:

```bash
./scripts/flash.sh -m          # flash and open the monitor
```

Type in the monitor:

| Key | Effect |
|---|---|
| `1`–`6` | show this state |
| `?` | help |

The demo runs by itself as long as the bridge has not checked in, and stops as
soon as it has — there is no switch for it any more.

**Checking touch:** select state `2`, then tap **Accept**. The monitor must print
`{"decision":"allow"}`. For **Deny**, `"deny"` accordingly.

**Checking gestures:** swipe from top to bottom across the screen — the usage view
appears. Swiping up goes back. If more than three sessions are running, the same
swipe pages through the list first; a narrow bar on the right shows where you are.
A tap on the tile on the left opens the detail view, another tap goes back.

**Clawd:** if no session is running, he walks across the screen — 13 s from left to
right, then a 5 s pause. He does not react to taps; the idle view belongs to the
swipe gestures.

## When something is wrong

**Display stays black, everything else runs.**
Then the board revision is wrong. This project is set up for **V2** (PCB silkscreen
`Rev1.1`, QC sticker `V2`). On V1, the backlight and LCD reset are on different
pins — see the comment header in
[user_config.h](firmware/claude_deck/user_config.h).

**Boot loop, the monitor says `PSRAM chip is not connected`.**
Then the PSRAM setting is on QSPI instead of octal. In
[scripts/common.sh](scripts/common.sh) it has to say `PSRAM=opi`, not `PSRAM=enabled`.
The ESP32-S3**R8** has octal PSRAM.

**`bad CPU type in executable` while compiling.**
The `ctags` that ships with Arduino is an x86 binary and does not run on the M2.
`./scripts/setup.sh` swaps it for a native one.

**No board found.**
Check the cable. It has to be a data cable, not a charge-only one.
`ls /dev/cu.usbmodem*` should print something.

**Touches do not arrive, or the device decides on its own.**
Should not happen any more, but if it does: the touch handling sits in
`TouchInputReadCallback` in [lvgl_port.c](firmware/claude_deck/lvgl_port.c).
Three safeguards with their reasoning are documented there — correct I²C bus, dead
zone against jitter, confirmation across two readings. To measure, compile with
`--build-property compiler.c.extra_flags=-DTOUCH_DEBUG=1`; the device then reports
every touch with coordinates over the monitor.

**Nothing helps.**
Plug the board in while holding the **BOOT** button (this forces the bootloader),
then `./scripts/flash.sh`.

## The Arduino IDE

You do not need it — the scripts do everything. If you want to open it anyway: the
sketch is `firmware/claude_deck`, and under *Tools* these have to be set:
board `ESP32S3 Dev Module`, **PSRAM: OPI PSRAM**, flash size `16MB`, partition
`3MB APP/9.9MB FATFS`, **USB CDC On Boot: Enabled**.

The code deliberately lives in `main.cpp`, not in the `.ino` — the reasoning is in
the header of [claude_deck.ino](firmware/claude_deck/claude_deck.ino).

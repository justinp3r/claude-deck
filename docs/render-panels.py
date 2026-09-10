#!/usr/bin/env python3
"""Erzeugt die Panel-Bilder fuer die README.

Kein Foto und kein Mockup: Koordinaten, Groessen und Farben sind direkt aus
firmware/claude_dashboard/ui_theme.h und ui_panel.c uebernommen, Clawd aus
clawd.c. Aendert sich die UI, gehoert dieses Skript mit angepasst und neu
ausgefuehrt:  python3 docs/render-panels.py
"""
import pathlib

W, H = 640, 172
BG          = "#08090A"
TILE        = "#141518"; TILE_BORDER = "#292B2F"
CHIP        = "#26282C"; CHIP_FG     = "#A9ACA8"
TEXT        = "#F0EFEC"; TEXT_BRIGHT = "#E6E5E1"; TEXT_MID = "#C9CCC7"
TEXT_DIM    = "#9A9E9A"; TEXT_GRAY   = "#8A8E8B"; TEXT_FAINT = "#6E7370"
TEXT_HEADER = "#5E635F"; RULE        = "#26282C"
ACCENT      = "#D97757"; ACCEPT_FG   = "#000000"; DENY_FG   = "#FFFFFF"
BAR_OK      = "#2563EB"; BAR_WARN    = "#F59E0B"; BAR_CRIT  = "#DC2626"
BAR_TRACK   = "#152A4E"
GREEN       = "#1D9E75"; AMBER       = "#EF9F27"; DOT_IDLE  = "#4A4F4C"

MONO = "IBM Plex Mono, ui-monospace, SFMono-Regular, Menlo, monospace"
SANS = "IBM Plex Sans, -apple-system, Segoe UI, Helvetica, Arial, sans-serif"

def esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))

def txt(x, y, s, size, fill, fam=MONO, anchor="start", weight="400"):
    """y ist die optische Mitte der Zeile, nicht die Grundlinie - so wie LVGL
    Labels mit _MID ausrichtet. 0.35*Groesse ist der uebliche Versatz."""
    return (f'<text x="{x}" y="{y + 0.35 * size:.1f}" font-family="{fam}" '
            f'font-size="{size}" font-weight="{weight}" fill="{fill}" '
            f'text-anchor="{anchor}">{esc(s)}</text>')

def rect(x, y, w, h, fill, r=0, stroke=None, sw=1):
    s = f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}"'
    if stroke: s += f' stroke="{stroke}" stroke-width="{sw}"'
    return s + "/>"

def clawd(ox, oy, width):
    """Aus clawd.c: Bounding-Box 378 x 253, neun Rechtecke."""
    SRC_W = 378
    sx = lambda v: round(v * width / SRC_W)
    p = []
    p.append(rect(ox + sx(60), oy, sx(259), sx(185), ACCENT))          # Rumpf
    p.append(rect(ox, oy + sx(61), sx(60), sx(64), ACCENT))            # Arm links
    p.append(rect(ox + sx(319), oy + sx(61), sx(59), sx(64), ACCENT))  # Arm rechts
    for lx, lw in ((60, 35), (124, 34), (219, 33), (282, 37)):         # Beine
        p.append(rect(ox + sx(lx), oy + sx(185), sx(lw), sx(68), ACCENT))
    for ex in (95, 249):                                               # Augen
        p.append(rect(ox + sx(ex), oy + sx(28), sx(34), sx(33), TILE))
    return p

def frame(body):
    return ('<svg xmlns="http://www.w3.org/2000/svg" '
            f'width="{W}" height="{H}" viewBox="0 0 {W} {H}" role="img">'
            + rect(0, 0, W, H, BG, r=9) + "".join(body) + "</svg>\n")

# ---------------------------------------------------------------- approval
def approval():
    p = []
    p.append(rect(10, 10, 152, 152, TILE, r=11, stroke=TILE_BORDER))
    cw = min(152 - 28, 112)
    p += clawd(10 + (152 - cw) // 2, 30, cw)
    # Name unten in der Kachel: LVGL richtet BOTTOM_MID mit -12 aus, die
    # Zeilenhoehe von Mono 18 ist 22 - Mitte also bei 162-12-11.
    p.append(txt(86, 139, "claude-deck", 18, "#B7BAB6", anchor="middle"))

    # Kontextspalte: Flex-Container y=12, 148 hoch, mittig, 6 px Abstand.
    # Kindhoehen 22 + 27 + 25 + 25 + 17 = 116, plus 4x6 = 140, also 4 px
    # Versatz oben. Daraus die Mittellinien der fuenf Zeilen.
    x = 178
    p.append(rect(x, 21, 12, 12, AMBER, r=6))
    p.append(txt(x + 22, 27, "Approval requested", 17, TEXT_MID, fam=SANS))
    p.append(rect(x, 44, 52, 27, CHIP, r=6))
    p.append(txt(x + 9, 57, "Bash", 14, CHIP_FG))
    p.append(txt(x, 89, "git push origin", 21, TEXT))
    p.append(txt(x, 120, "main --force", 21, TEXT))
    p.append(txt(x, 147, "a4f1 · 12 s", 14, TEXT_FAINT))

    p.append(rect(462, 10, 168, 66, ACCENT, r=10))
    p.append(txt(546, 43, "Accept", 23, ACCEPT_FG, fam=SANS, anchor="middle", weight="600"))
    p.append(rect(462, 96, 168, 66, "none", r=10, stroke=ACCENT, sw=2))
    p.append(txt(546, 129, "Deny", 23, DENY_FG, fam=SANS, anchor="middle", weight="600"))
    return frame(p)

# -------------------------------------------------------------------- idle
def idle():
    rows = [("backend-refactor", "14 m", True),
            ("ipod-website",     "3 m",  True),
            ("docs-sweep",       "22 m", False)]
    p = [txt(20, 22, "3 sessions", 16, TEXT_HEADER),
         txt(620, 22, "21:14", 16, TEXT_HEADER, anchor="end")]
    for i, (name, age, on) in enumerate(rows):
        cy = 46 + i * 39 + 17                      # Zeile 34 hoch, 5 Abstand
        p.append(rect(20, cy - 5, 10, 10, GREEN if on else DOT_IDLE, r=5))
        p.append(txt(46, cy, name, 21, TEXT_BRIGHT if on else TEXT_GRAY))
        p.append(txt(610, cy, age, 18, TEXT_FAINT, anchor="end"))
    return frame(p)

# ------------------------------------------------------------------- usage
def usage():
    p = [txt(20, 22, "Usage", 16, TEXT_HEADER),
         txt(620, 22, "Opus 5", 16, TEXT_HEADER, anchor="end"),
         rect(20, 40, W - 40, 1, RULE)]
    for i, (name, pct, reset) in enumerate((("5-hour", 82, "2h 14m left"),
                                            ("Weekly", 38, "4d 6h left"))):
        y = 72 + i * 44
        col = BAR_CRIT if pct >= 95 else BAR_WARN if pct >= 75 else BAR_OK
        p.append(txt(20, y + 9, name, 16, TEXT_DIM))
        p.append(rect(112, y + 4, 286, 12, BAR_TRACK, r=6))
        p.append(rect(112, y + 4, round(286 * pct / 100), 12, col, r=6))
        p.append(txt(452, y + 10, f"{pct}%", 18, TEXT, anchor="end"))
        p.append(txt(620, y + 9, reset, 14, TEXT_FAINT, anchor="end"))
    return frame(p)

out = pathlib.Path(__file__).parent / "img"
out.mkdir(exist_ok=True)
for name, fn in (("panel-approval", approval), ("panel-idle", idle), ("panel-usage", usage)):
    (out / f"{name}.svg").write_text(fn())
    print("geschrieben:", (out / f"{name}.svg").relative_to(pathlib.Path.cwd()))

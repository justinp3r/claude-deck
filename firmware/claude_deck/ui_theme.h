/* Design-Tokens aus dem urspruenglichen Entwurf (siehe CLAUDE.md, Abschnitt
 * Design; die Entwurfsdatei selbst liegt nur noch in der Git-Historie).
 *
 * Das Panel ist 640 x 172 px auf 84,0 x 22,6 mm, also 7,62 px/mm.
 * Daraus folgt die Regel, die alles andere bestimmt: alles Antippbare
 * misst mindestens 66 px Hoehe (8,7 mm). In der Hoehe passen damit genau
 * zwei Reihen uebereinander. Eine dritte gibt es nicht.
 */
#ifndef UI_THEME_H
#define UI_THEME_H

/* --- Flaeche --- */
#define UI_W                640
#define UI_H                172
#define UI_PAD_X            20    /* Seitenrand */

/* --- Farben ---
 * Bernstein heisst wartet, Tuerkis heisst laeuft, Rot heisst kaputt.
 * Aus zwei Metern liest man auf 22,6 mm Hoehe keinen Text mehr, nur Farbe. */
#define UI_C_BG             0x08090A  /* Grundflaeche */
#define UI_C_TILE           0x141518  /* Kachel-Hintergrund */
#define UI_C_TILE_BORDER    0x292B2F
#define UI_C_AVATAR_BG      0x1D3A5C
#define UI_C_AVATAR_FG      0x8CB6E4
#define UI_C_CHIP           0x26282C  /* Tool-Chip */
#define UI_C_CHIP_FG        0xA9ACA8
#define UI_C_RULE           0x26282C

#define UI_C_TEXT           0xF0EFEC  /* primaer, der Befehl selbst */
#define UI_C_TEXT_BRIGHT    0xE6E5E1
#define UI_C_TEXT_MID       0xC9CCC7
#define UI_C_TEXT_DIM       0x9A9E9A
#define UI_C_TEXT_GRAY      0x8A8E8B
#define UI_C_TEXT_FAINT     0x6E7370
#define UI_C_TEXT_HEADER    0x5E635F

/* Beide Tasten tragen denselben Akzent. Unterschieden wird ueber das
 * Gewicht, nicht ueber den Farbton: Accept ist gefuellt, Deny nur umrissen. */
#define UI_C_ACCENT         0xD97757  /* Anthropic-Orange, beide Tasten */
#define UI_C_ACCEPT_FG      0x000000  /* Schrift auf der gefuellten Taste */
#define UI_C_DENY_FG        0xFFFFFF  /* Schrift auf der umrissenen Taste */

/* Verbrauchsbalken. Schwellen aus der Claude-Code-Doku (Nutzungswarnungen):
 * gewarnt wird ab 75 % und erneut ab 95 % der Auslastung. Darunter blau wie in
 * der Weboberflaeche, darueber die beiden Warnstufen.
 * Blau und Rot stammen aus der Farbpalette im Claude-Code-Bundle. */
#define UI_C_BAR_OK         0x2563EB  /* < 75 %  */
#define UI_C_BAR_WARN       0xF59E0B  /* >= 75 % */
#define UI_C_BAR_CRIT       0xDC2626  /* >= 95 % */
#define UI_C_BAR_TRACK      0x152A4E  /* unausgefuellter Teil, dunkles Marineblau */
#define UI_BAR_WARN_PCT     75
#define UI_BAR_CRIT_PCT     95

#define UI_C_GREEN          0x1D9E75  /* laeuft - Punkt im Ruhezustand */
#define UI_C_AMBER          0xEF9F27  /* wartet */
#define UI_C_RED_BAR        0xA32D2D  /* Steg im Getrennt-Zustand */
#define UI_C_RED_SOFT       0xF09595  /* Ueberschrift im Getrennt-Zustand */
#define UI_C_ORANGE         0xE27A4A  /* Warnung im Detail */
#define UI_C_DOT_IDLE       0x4A4F4C
#define UI_C_QUEUE_IDLE     0x3A3D3B

/* --- Masse --- */
#define UI_TILE_X           10
#define UI_TILE_Y           10
#define UI_TILE_SIZE        152
#define UI_AVATAR_SIZE      80

#define UI_BTN_X            462   /* linke Kante beider Tasten */
#define UI_BTN_W            168
#define UI_BTN_H            66    /* 8,7 mm - die Untergrenze */
#define UI_BTN_TOP_Y        10
#define UI_BTN_BOT_Y        96    /* 20 px Abstand zwischen den Tasten */

#define UI_COL_MID_X        178   /* Beginn der Kontextspalte */

#endif /* UI_THEME_H */

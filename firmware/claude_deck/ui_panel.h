/* Die sechs Zustaende des Panels. Ein Zustand pro Bild: jeder liegt in einem
 * eigenen Container, sichtbar ist immer genau einer. */
#ifndef UI_PANEL_H
#define UI_PANEL_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_STATE_IDLE = 0,       /* ruhe          - nichts zu tun */
    UI_STATE_APPROVAL,       /* freigabe      - Accept und Deny */
    UI_STATE_QUEUE,          /* warteschlange - mehr als einer fragt */
    UI_STATE_DETAIL,         /* detail        - der ganze Befehl */
    UI_STATE_USAGE,          /* usage         - Limits, per Wisch nach unten */
    UI_STATE_DISCONNECTED,   /* getrennt      - Bridge weg */
    UI_STATE_COUNT
} ui_state_t;

/* Wie die Entscheidung gefallen ist. Wird an den Callback gereicht. */
typedef enum {
    UI_DECISION_ALLOW = 0,
    UI_DECISION_DENY
} ui_decision_t;

/* Wird gerufen, wenn der Nutzer entschieden hat. Laeuft im LVGL-Task. */
typedef void (*ui_decision_cb_t)(ui_decision_t decision);

/* Baut alle Zustaende auf. Nur aus dem LVGL-Task oder unter lvgl_port_lock(). */
void ui_panel_init(void);

void ui_panel_show(ui_state_t state);
ui_state_t ui_panel_current(void);

void ui_panel_set_decision_cb(ui_decision_cb_t cb);

/* --- Inhalte setzen --- */

/* Kopfzeile im Ruhezustand, z.B. ("3 sessions", "21:14"). */
void ui_panel_set_idle_header(const char *left, const char *right);

/* Drei Zeilen sind sichtbar, der Rest wird durch Wischen erreicht. */
#define UI_IDLE_ROWS     9
#define UI_IDLE_PER_PAGE 3

/* Ein leerer name blendet die Zeile komplett aus - Punkt inklusive. */
void ui_panel_set_idle_row(int slot, bool active, const char *name, const char *age);

/* Freigabeanfrage: Name unter der Kachel, Tool-Chip, Befehl zweizeilig
 * (line2 darf NULL sein), meta z.B. "a4f1 - 12 s". */
void ui_panel_set_request(const char *agent, const char *tool,
                          const char *line1, const char *line2, const char *meta);

/* Kopfzeile der Warteschlange, z.B. (1, 2, "docs-sweep"). */
void ui_panel_set_queue(int position, int total, const char *next_agent);

/* Detailansicht. warning darf NULL sein. */
void ui_panel_set_detail(const char *header, const char *command,
                         const char *cwd, const char *consequence, const char *warning);

/* Getrennt-Zustand. */
void ui_panel_set_disconnected(const char *headline, const char *body, const char *meta);

/* Sekundenzaehler im Kopf der Freigabe. */
void ui_panel_set_countdown(int seconds);

/* Prozente 0..100, -1 heisst "dieses Fenster fehlt". Resetzeiten kommen
 * fertig formatiert von der Bridge. Nur die beiden kontoweiten Fenster. */
void ui_panel_set_usage(bool have,
                        int five_pct,  const char *five_reset,
                        int week_pct,  const char *week_reset);

/* Wischen links/rechts: naechste bzw. vorige offene Anfrage. */
typedef void (*ui_focus_cb_t)(int delta);
void ui_panel_set_focus_cb(ui_focus_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif /* UI_PANEL_H */

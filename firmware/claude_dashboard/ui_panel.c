/* Alle fuenf Zustaende des Freigabepanels, siehe agent-panel-design.html.
 *
 * Saemtliche Texte auf dem Display sind Englisch, die Kommentare Deutsch.
 *
 * Aufbau: ein Screen, darauf fuenf Container gleicher Groesse (640 x 172).
 * Genau einer ist sichtbar, die anderen tragen LV_OBJ_FLAG_HIDDEN. Das
 * kostet etwas Speicher, spart aber jedes Neuaufbauen beim Wechsel und
 * haelt die Regel "ein Zustand pro Bild" strukturell durch.
 *
 * Zur Positionierung: die SVGs im Design geben Text ueber die Grundlinie an,
 * LVGL ueber die obere Kante. Statt Grundlinien umzurechnen (was bei jedem
 * Fontwechsel wieder bricht) sind Textstapel hier in Flex-Container gelegt
 * und mittig ausgerichtet. Das zentriert optisch korrekt, unabhaengig von
 * den Metriken der Schrift.
 */

#include "lvgl.h"
#include "ui_panel.h"
#include "ui_theme.h"
#include "clawd.h"
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(ui_font_mono_14);
LV_FONT_DECLARE(ui_font_mono_16);
LV_FONT_DECLARE(ui_font_mono_18);
LV_FONT_DECLARE(ui_font_mono_21);
LV_FONT_DECLARE(ui_font_mono_30);
LV_FONT_DECLARE(ui_font_sans_17);
LV_FONT_DECLARE(ui_font_sans_sb_23);
LV_FONT_DECLARE(ui_font_sans_sb_26);

/* ------------------------------------------------------------------ */
/* Zustand                                                             */
/* ------------------------------------------------------------------ */

static lv_obj_t *s_layer[UI_STATE_COUNT];
static ui_state_t s_current = UI_STATE_IDLE;
static ui_decision_cb_t s_decision_cb = NULL;

static lv_obj_t *s_idle_left, *s_idle_right;
static lv_obj_t *s_idle_row[UI_IDLE_ROWS], *s_idle_dot[UI_IDLE_ROWS];
static lv_obj_t *s_idle_name[UI_IDLE_ROWS], *s_idle_age[UI_IDLE_ROWS];
static lv_obj_t *s_idle_list, *s_idle_thumb, *s_idle_track, *s_idle_stroll;
static int s_idle_count = 0;   /* belegte Zeilen */
static int s_idle_page  = 0;

typedef struct {
    lv_obj_t *agent, *tool, *line1, *line2, *meta, *status;
} request_view_t;

static request_view_t s_req;     /* Freigabe */
static request_view_t s_req_q;   /* Warteschlange */

static lv_obj_t *s_queue_bar[2];
static lv_obj_t *s_detail_header, *s_detail_cmd, *s_detail_cwd,
                *s_detail_conseq, *s_detail_warn;
static lv_obj_t *s_disc_head, *s_disc_body, *s_disc_meta;

static char s_meta_base[64] = "a4f1";
static int  s_countdown = 12;

/* Usage-Ansicht */
static lv_obj_t *s_usage_model, *s_usage_none, *s_usage_rows;
static lv_obj_t *s_bar_fill[2], *s_bar_pct[2], *s_bar_reset[2];

/* Gesten. Eine erkannte Geste darf nicht zusaetzlich als Klick durchgehen -
 * LVGL unterdrueckt den Klick von sich aus NICHT. Sonst wuerde ein Wisch, der
 * auf Accept beginnt, die Freigabe ausloesen. */
static bool          s_gesture_in_press = false;
static ui_state_t    s_before_usage = UI_STATE_IDLE;
static ui_focus_cb_t s_focus_cb = NULL;

/* ------------------------------------------------------------------ */
/* Bausteine                                                           */
/* ------------------------------------------------------------------ */

/* Nackter Container: kein Hintergrund, kein Rand, kein Padding, kein Scrollen.
 * lv_obj_create() bringt per Default Theme-Styling mit, das hier ueberall
 * stoert - deshalb einmal zentral abraeumen. */
static lv_obj_t *bare(lv_obj_t *parent, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    /* lv_obj_create setzt LV_OBJ_FLAG_CLICKABLE von sich aus (lv_obj.c:
     * "obj->flags = LV_OBJ_FLAG_CLICKABLE"). Damit faengt jedes Deko-Rechteck
     * Beruehrungen ab, die dem Elternteil galten - der Treffertest liefert
     * immer das oberste getroffene Kind. Hier also wieder abschalten und nur
     * dort gezielt setzen, wo wirklich etwas passieren soll. */
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t *label(lv_obj_t *parent, const char *txt,
                       const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt ? txt : "");
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

static lv_obj_t *dot(lv_obj_t *parent, int size, uint32_t color)
{
    lv_obj_t *d = bare(parent, size, size);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(d, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    return d;
}

/* Tool-Chip, z.B. [Bash]. Waechst mit dem Text. */
static lv_obj_t *chip(lv_obj_t *parent, const char *txt)
{
    lv_obj_t *c = label(parent, txt, &ui_font_mono_14, UI_C_CHIP_FG);
    lv_obj_set_style_bg_color(c, lv_color_hex(UI_C_CHIP), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 6, 0);
    lv_obj_set_style_pad_hor(c, 9, 0);
    lv_obj_set_style_pad_ver(c, 5, 0);
    return c;
}

/* Die Agentenkachel links: Clawd, darunter der Name der Session. */
static void build_tile(lv_obj_t *parent, int x, int w, request_view_t *v)
{
    lv_obj_t *tile = bare(parent, w, UI_TILE_SIZE);
    lv_obj_set_pos(tile, x, UI_TILE_Y);
    lv_obj_set_style_bg_color(tile, lv_color_hex(UI_C_TILE), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tile, lv_color_hex(UI_C_TILE_BORDER), 0);
    lv_obj_set_style_border_width(tile, 1, 0);
    lv_obj_set_style_radius(tile, 11, 0);
    lv_obj_set_style_clip_corner(tile, true, 0);

    int cw = w - 28;
    if (cw > 112) cw = 112;
    lv_obj_t *clawd = clawd_create(tile, cw);
    lv_obj_align(clawd, LV_ALIGN_TOP_MID, 0, 20);

    /* Sessionnamen sind oft lang ("claude-dashboard"). Ohne feste Breite
     * waechst das Label mit LV_SIZE_CONTENT ueber den Kachelrand hinaus. */
    v->agent = label(tile, "backend", &ui_font_mono_18, 0xB7BAB6);
    lv_obj_set_width(v->agent, w - 16);
    lv_label_set_long_mode(v->agent, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(v->agent, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(v->agent, LV_ALIGN_BOTTOM_MID, 0, -12);
}

/* Kontextspalte: Status, Chip, Befehl, Meta - als Stapel mittig in der Hoehe. */
static void build_context(lv_obj_t *parent, int x, int w,
                          request_view_t *v, bool amber_dot)
{
    lv_obj_t *col = bare(parent, w, UI_H - 24);
    lv_obj_set_pos(col, x, 12);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, 6, 0);

    lv_obj_t *row = bare(col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 10, 0);

    if (amber_dot) {
        lv_obj_t *d = dot(row, 12, UI_C_AMBER);
        /* Atmen: macht aus zwei Metern sichtbar, dass etwas wartet. */
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, d);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_30);
        lv_anim_set_duration(&a, 950);
        lv_anim_set_playback_duration(&a, 950);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_bg_opa);
        lv_anim_start(&a);
        v->status = label(row, "Approval requested", &ui_font_sans_17, UI_C_TEXT_MID);
    } else {
        v->status = label(row, "1 of 2 waiting", &ui_font_sans_17, UI_C_AMBER);
    }

    v->tool  = chip(col, "Bash");
    v->line1 = label(col, "git push origin", &ui_font_mono_21, UI_C_TEXT);
    v->line2 = label(col, "main --force",    &ui_font_mono_21, UI_C_TEXT);
    v->meta  = label(col, "a4f1 \xC2\xB7 12 s", &ui_font_mono_14, UI_C_TEXT_FAINT);

    /* Echte Befehle sind oft laenger als die Spalte. Ohne feste Breite waechst
     * ein Label mit LV_SIZE_CONTENT einfach weiter und schiebt sich unter die
     * Tasten. Deshalb hier abschneiden - die volle Zeile zeigt die Detailansicht. */
    lv_obj_t *clipped[4] = { v->status, v->line1, v->line2, v->meta };
    for (int i = 0; i < 4; i++) {
        lv_obj_set_width(clipped[i], w);
        lv_label_set_long_mode(clipped[i], LV_LABEL_LONG_DOT);
    }
    /* Die Statuszeile sitzt in der Punkt-Zeile und darf nur den Rest belegen. */
    lv_obj_set_width(v->status, w - 22);
}

/* ------------------------------------------------------------------ */
/* Tasten                                                              */
/* ------------------------------------------------------------------ */

static void decide(ui_decision_t d)
{
    if (s_decision_cb) s_decision_cb(d);
}

/* Beginnt ein neuer Druck, ist noch keine Geste gelaufen. */
static void clear_gesture_cb(lv_event_t *e) { LV_UNUSED(e); s_gesture_in_press = false; }

/* An jedem antippbaren Element: Druckbeginn merken, damit ein Wisch, der hier
 * startet, nicht als Tipp endet. */
static void guard_taps(lv_obj_t *obj)
{
    lv_obj_add_event_cb(obj, clear_gesture_cb, LV_EVENT_PRESSED, NULL);
}

static void accept_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (s_gesture_in_press) return;
    decide(UI_DECISION_ALLOW);
}

static void deny_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (s_gesture_in_press) return;
    decide(UI_DECISION_DENY);
}

/* Grundform beider Tasten. 168 x 66 px, also 22,0 x 8,7 mm - die Untergrenze,
 * unterhalb derer man nicht mehr sicher trifft. */
static lv_obj_t *button_base(lv_obj_t *parent, int y, const char *txt,
                             uint32_t fg, lv_event_cb_t cb)
{
    lv_obj_t *b = bare(parent, UI_BTN_W, UI_BTN_H);
    lv_obj_set_pos(b, UI_BTN_X, y);
    lv_obj_set_style_radius(b, 10, 0);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *l = label(b, txt, &ui_font_sans_sb_23, fg);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    guard_taps(b);
    return b;
}

/* Accept: volle Flaeche im Akzentton, schwarze Schrift. */
static lv_obj_t *accept_button(lv_obj_t *parent, int y)
{
    lv_obj_t *b = button_base(parent, y, "Accept", UI_C_ACCEPT_FG, accept_cb);
    lv_obj_set_style_bg_color(b, lv_color_hex(UI_C_ACCENT), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    return b;
}

/* Deny: nur umrissen, weisse Schrift. */
static lv_obj_t *deny_button(lv_obj_t *parent, int y)
{
    lv_obj_t *b = button_base(parent, y, "Deny", UI_C_DENY_FG, deny_cb);
    lv_obj_set_style_border_color(b, lv_color_hex(UI_C_ACCENT), 0);
    lv_obj_set_style_border_width(b, 2, 0);
    return b;
}

/* ------------------------------------------------------------------ */
/* Die fuenf Zustaende                                                 */
/* ------------------------------------------------------------------ */

/* ruhe - der haeufigste Zustand. Keine Tasten, keine Kacheln, kein Rahmen.
 * Nur wer laeuft und wie lange schon. */
/* Eine Zeile ist 34 hoch, 5 Abstand. Drei davon passen in 112 px - genau das
 * ist ein "Blatt". Mehr Sessions verschieben die Liste blattweise. */
#define IDLE_ROW_H   34
#define IDLE_ROW_GAP  5
#define IDLE_PAGE_H  (UI_IDLE_PER_PAGE * IDLE_ROW_H + (UI_IDLE_PER_PAGE - 1) * IDLE_ROW_GAP)
#define IDLE_STEP    (IDLE_PAGE_H + IDLE_ROW_GAP)
#define IDLE_LIST_W  (UI_W - 2 * UI_PAD_X - 10)   /* 10 px fuer den Positionssteg */

static int idle_pages(void)
{
    if (s_idle_count <= 0) return 1;
    return (s_idle_count + UI_IDLE_PER_PAGE - 1) / UI_IDLE_PER_PAGE;
}

/* Positionssteg rechts: zeigt, dass es weitergeht. Kein Bedienelement,
 * sondern dieselbe Sprache wie die Warteschlangenleiste im Design. */
static void idle_apply_page(bool animate)
{
    int pages = idle_pages();
    if (s_idle_page >= pages) s_idle_page = pages - 1;
    if (s_idle_page < 0) s_idle_page = 0;

    int y = -s_idle_page * IDLE_STEP;
    if (animate) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_idle_list);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_values(&a, lv_obj_get_y(s_idle_list), y);
        lv_anim_set_duration(&a, 180);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    } else {
        lv_obj_set_y(s_idle_list, y);
    }

    if (pages > 1) {
        lv_obj_remove_flag(s_idle_track, LV_OBJ_FLAG_HIDDEN);
        int h = IDLE_PAGE_H / pages;
        if (h < 12) h = 12;
        lv_obj_set_height(s_idle_thumb, h);
        lv_obj_set_y(s_idle_thumb, (IDLE_PAGE_H - h) * s_idle_page / (pages - 1));
    } else {
        lv_obj_add_flag(s_idle_track, LV_OBJ_FLAG_HIDDEN);
    }
}

/* true, wenn die Geste hier verbraucht wurde. delta > 0 = spaetere Eintraege. */
static bool idle_scroll(int delta)
{
    if (s_current != UI_STATE_IDLE) return false;
    int pages = idle_pages();
    int want = s_idle_page + delta;
    if (want < 0 || want >= pages) return false;
    s_idle_page = want;
    idle_apply_page(true);
    return true;
}

static void build_idle(lv_obj_t *root)
{
    /* Ohne Bridge ist genau das der Zustand: nichts laeuft. Ein frisch
     * geflashtes Geraet soll das sagen, nicht leer bleiben. */
    s_idle_left = label(root, "no sessions", &ui_font_mono_16, UI_C_TEXT_HEADER);
    lv_obj_set_pos(s_idle_left, UI_PAD_X, 14);

    s_idle_right = label(root, "", &ui_font_mono_16, UI_C_TEXT_HEADER);
    lv_obj_align(s_idle_right, LV_ALIGN_TOP_RIGHT, -UI_PAD_X, 14);

    /* Sichtfenster: schneidet ab, was nicht auf das Blatt passt.
     * LVGL beschneidet Kinder am Elternrand, solange OVERFLOW_VISIBLE aus ist. */
    lv_obj_t *view = bare(root, IDLE_LIST_W, IDLE_PAGE_H);
    lv_obj_set_pos(view, UI_PAD_X, 46);

    s_idle_list = bare(view, IDLE_LIST_W, UI_IDLE_ROWS * IDLE_STEP);
    lv_obj_set_pos(s_idle_list, 0, 0);

    for (int i = 0; i < UI_IDLE_ROWS; i++) {
        lv_obj_t *row = bare(s_idle_list, IDLE_LIST_W, IDLE_ROW_H);
        lv_obj_set_pos(row, 0, i * (IDLE_ROW_H + IDLE_ROW_GAP));
        s_idle_row[i] = row;

        s_idle_dot[i] = dot(row, 10, UI_C_DOT_IDLE);
        lv_obj_align(s_idle_dot[i], LV_ALIGN_LEFT_MID, 0, 0);

        s_idle_name[i] = label(row, "", &ui_font_mono_21, UI_C_TEXT_BRIGHT);
        /* Platz fuer den Punkt links und die Dauer rechts freihalten. */
        lv_obj_set_width(s_idle_name[i], IDLE_LIST_W - 26 - 96);
        lv_label_set_long_mode(s_idle_name[i], LV_LABEL_LONG_DOT);
        lv_obj_align(s_idle_name[i], LV_ALIGN_LEFT_MID, 26, 0);

        s_idle_age[i] = label(row, "", &ui_font_mono_18, UI_C_TEXT_FAINT);
        lv_obj_align(s_idle_age[i], LV_ALIGN_RIGHT_MID, 0, 0);

        /* Bis Daten kommen, ist nichts da - also auch kein Punkt. */
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    }

    /* Ist nichts los, laeuft Clawd durch die Leere. Nur dann - sobald eine
     * Session da ist, gehoert der Platz der Liste. */
    s_idle_stroll = clawd_create(root, 140);
    lv_obj_set_pos(s_idle_stroll, -140, 58);
    {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_idle_stroll);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
        lv_anim_set_values(&a, -140, UI_W);
        lv_anim_set_duration(&a, 13000);         /* gemuetlich, nicht hektisch */
        lv_anim_set_repeat_delay(&a, 5000);      /* danach kurz weg vom Schirm */
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }

    s_idle_track = bare(root, 3, IDLE_PAGE_H);
    lv_obj_set_pos(s_idle_track, UI_W - UI_PAD_X - 3, 46);
    lv_obj_set_style_radius(s_idle_track, 2, 0);
    lv_obj_set_style_bg_color(s_idle_track, lv_color_hex(UI_C_CHIP), 0);
    lv_obj_set_style_bg_opa(s_idle_track, LV_OPA_COVER, 0);

    s_idle_thumb = bare(s_idle_track, 3, 20);
    lv_obj_set_style_radius(s_idle_thumb, 2, 0);
    lv_obj_set_style_bg_color(s_idle_thumb, lv_color_hex(UI_C_QUEUE_IDLE), 0);
    lv_obj_set_style_bg_opa(s_idle_thumb, LV_OPA_COVER, 0);

    lv_obj_add_flag(s_idle_track, LV_OBJ_FLAG_HIDDEN);
}

/* freigabe - Kachel links, Kontext mittig, Entscheidung rechts. */
static void build_approval(lv_obj_t *root)
{
    build_tile(root, UI_TILE_X, UI_TILE_SIZE, &s_req);
    build_context(root, UI_COL_MID_X, UI_BTN_X - UI_COL_MID_X - 12, &s_req, true);
    accept_button(root, UI_BTN_TOP_Y);
    deny_button(root,   UI_BTN_BOT_Y);
}

/* warteschlange - der Zustand, der beim Bauen gern vergessen wird und im
 * Betrieb sofort auftritt. Die Leiste links zeigt, wie viele warten. */
static void build_queue(lv_obj_t *root)
{
    for (int i = 0; i < 2; i++) {
        s_queue_bar[i] = bare(root, 8, 54);
        lv_obj_set_pos(s_queue_bar[i], 10, 26 + i * 66);
        lv_obj_set_style_radius(s_queue_bar[i], 4, 0);
        lv_obj_set_style_bg_color(s_queue_bar[i],
            lv_color_hex(i == 0 ? UI_C_AMBER : UI_C_QUEUE_IDLE), 0);
        lv_obj_set_style_bg_opa(s_queue_bar[i], LV_OPA_COVER, 0);
    }

    build_tile(root, 34, 140, &s_req_q);
    build_context(root, 192, UI_BTN_X - 192 - 12, &s_req_q, false);
    lv_label_set_text(s_req_q.tool, "Edit");
    lv_label_set_text(s_req_q.line1, "src/auth/token.py");
    lv_label_set_text(s_req_q.line2, "+42 \xE2\x88\x92""17");
    lv_obj_set_style_text_font(s_req_q.line2, &ui_font_mono_18, 0);
    lv_obj_set_style_text_color(s_req_q.line2, lv_color_hex(UI_C_TEXT_DIM), 0);
    lv_label_set_text(s_req_q.meta, "a4f1 \xC2\xB7 4 s \xC2\xB7 then docs-sweep");

    accept_button(root, UI_BTN_TOP_Y);
    deny_button(root,   UI_BTN_BOT_Y);
}

/* detail - der ganze Befehl. Hier ist die volle Breite die einzige Ressource,
 * also verschwindet alles andere. Warnung zuletzt, nicht zuerst. */
static void build_detail(lv_obj_t *root)
{
    s_detail_header = label(root, "Bash \xC2\xB7 backend-refactor",
                            &ui_font_mono_14, UI_C_TEXT_FAINT);
    lv_obj_set_pos(s_detail_header, UI_PAD_X, 14);

    lv_obj_t *hint = label(root, "tap to close",
                           &ui_font_mono_14, UI_C_TEXT_FAINT);
    lv_obj_align(hint, LV_ALIGN_TOP_RIGHT, -UI_PAD_X, 14);

    lv_obj_t *rule = bare(root, UI_W - 2 * UI_PAD_X, 1);
    lv_obj_set_pos(rule, UI_PAD_X, 40);
    lv_obj_set_style_bg_color(rule, lv_color_hex(UI_C_RULE), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);

    lv_obj_t *col = bare(root, UI_W - 2 * UI_PAD_X, UI_H - 40 - 14);
    lv_obj_set_pos(col, UI_PAD_X, 44);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, 7, 0);

    s_detail_cmd    = label(col, "git push origin main --force", &ui_font_mono_21, UI_C_TEXT);
    s_detail_cwd    = label(col, "cwd ~/dev/api", &ui_font_mono_16, UI_C_TEXT_DIM);
    s_detail_conseq = label(col, "3 commits on origin will be overwritten",
                            &ui_font_mono_16, UI_C_TEXT_DIM);
    s_detail_warn   = label(col, "no backup branch",
                            &ui_font_mono_14, UI_C_ORANGE);

    const int cw = UI_W - 2 * UI_PAD_X;
    /* Hier ist die volle Breite die einzige Ressource, die es gibt: der Befehl
     * darf umbrechen (dafuer ist diese Ansicht da), alles andere wird gekuerzt. */
    lv_obj_set_width(s_detail_cmd, cw);
    lv_label_set_long_mode(s_detail_cmd, LV_LABEL_LONG_WRAP);
    lv_obj_t *dclip[3] = { s_detail_cwd, s_detail_conseq, s_detail_warn };
    for (int i = 0; i < 3; i++) {
        lv_obj_set_width(dclip[i], cw);
        lv_label_set_long_mode(dclip[i], LV_LABEL_LONG_DOT);
    }
}

/* getrennt - das Geraet weiss nicht mehr, was laeuft. Es sagt, was passiert
 * ist und wo die Freigaben jetzt landen. Kein Ausrufezeichen. */
static void build_disconnected(lv_obj_t *root)
{
    lv_obj_t *bar = bare(root, 6, UI_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_radius(bar, 3, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(UI_C_RED_BAR), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    lv_obj_t *col = bare(root, UI_W - 68, UI_H - 24);
    lv_obj_set_pos(col, 34, 12);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, 12, 0);

    s_disc_head = label(col, "Bridge unreachable", &ui_font_sans_sb_26, UI_C_RED_SOFT);
    s_disc_body = label(col, "Approvals continue in the terminal.",
                        &ui_font_sans_17, 0xB7BAB6);
    s_disc_meta = label(col, "last contact 21:09 \xC2\xB7 check USB",
                        &ui_font_mono_16, UI_C_TEXT_FAINT);
}

/* usage - was das Zeitfenster und die Woche noch hergeben.
 * Erreichbar durch Wischen nach unten, zurueck durch Wischen nach oben.
 * Bewusst ohne sichtbare Schaltflaeche: auf 22,6 mm Hoehe ist jedes
 * Bedienelement Platz, der dem Inhalt fehlt. */

#define BAR_X     112
#define BAR_W     286
#define BAR_H      12
#define PCT_R     452      /* rechte Kante der Prozentzahl */
#define USAGE_ROWS 2       /* 5-Stunden-Fenster und Woche - beide kontoweit */
#define ROW_STEP   44

static uint32_t load_color(int pct)
{
    if (pct >= UI_BAR_CRIT_PCT) return UI_C_BAR_CRIT;
    if (pct >= UI_BAR_WARN_PCT) return UI_C_BAR_WARN;
    return UI_C_BAR_OK;
}

static void build_usage(lv_obj_t *root)
{
    lv_obj_t *hdr = label(root, "Usage", &ui_font_mono_16, UI_C_TEXT_HEADER);
    lv_obj_set_pos(hdr, UI_PAD_X, 14);

    s_usage_model = label(root, "", &ui_font_mono_16, UI_C_TEXT_HEADER);
    lv_obj_align(s_usage_model, LV_ALIGN_TOP_RIGHT, -UI_PAD_X, 14);

    lv_obj_t *rule = bare(root, UI_W - 2 * UI_PAD_X, 1);
    lv_obj_set_pos(rule, UI_PAD_X, 40);
    lv_obj_set_style_bg_color(rule, lv_color_hex(UI_C_RULE), 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);

    /* Ohne Daten: eine ehrliche Zeile statt leerer Balken. */
    s_usage_none = label(root,
        "No limit data yet available. You need a Pro or Max plan\nand one reply in this session",
        &ui_font_sans_17, UI_C_TEXT_DIM);
    lv_obj_set_style_text_align(s_usage_none, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_usage_none, LV_ALIGN_CENTER, 0, 12);

    /* Zwei Zeilen statt vormals drei: der Block waere sonst kopflastig. Er
     * belegt ROW_STEP + Zeilenhoehe, also rund 63 px; mittig zwischen Linie
     * (y=40) und Unterkante bleiben oben wie unten je 34 px. */
    s_usage_rows = bare(root, UI_W, UI_H - 72);
    lv_obj_set_pos(s_usage_rows, 0, 72);

    static const char *names[USAGE_ROWS] = { "5-hour", "Weekly" };
    for (int i = 0; i < USAGE_ROWS; i++) {
        int y = i * ROW_STEP;

        lv_obj_t *n = label(s_usage_rows, names[i], &ui_font_mono_16, UI_C_TEXT_DIM);
        lv_obj_set_pos(n, UI_PAD_X, y);

        lv_obj_t *track = bare(s_usage_rows, BAR_W, BAR_H);
        lv_obj_set_pos(track, BAR_X, y + 4);
        lv_obj_set_style_radius(track, BAR_H / 2, 0);
        lv_obj_set_style_bg_color(track, lv_color_hex(UI_C_BAR_TRACK), 0);
        lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);

        s_bar_fill[i] = bare(track, 0, BAR_H);
        lv_obj_set_pos(s_bar_fill[i], 0, 0);
        lv_obj_set_style_radius(s_bar_fill[i], BAR_H / 2, 0);
        lv_obj_set_style_bg_color(s_bar_fill[i], lv_color_hex(UI_C_BAR_OK), 0);
        lv_obj_set_style_bg_opa(s_bar_fill[i], LV_OPA_COVER, 0);

        s_bar_pct[i] = label(s_usage_rows, "\xE2\x80\x94", &ui_font_mono_18, UI_C_TEXT);
        lv_obj_set_width(s_bar_pct[i], 60);
        lv_obj_set_style_text_align(s_bar_pct[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_pos(s_bar_pct[i], PCT_R - 60, y - 1);

        s_bar_reset[i] = label(s_usage_rows, "", &ui_font_mono_14, UI_C_TEXT_FAINT);
        lv_obj_set_width(s_bar_reset[i], 140);
        lv_obj_set_style_text_align(s_bar_reset[i], LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(s_bar_reset[i], LV_LABEL_LONG_DOT);
        lv_obj_set_pos(s_bar_reset[i], UI_W - UI_PAD_X - 140, y + 1);
    }
    lv_obj_add_flag(s_usage_rows, LV_OBJ_FLAG_HIDDEN);
}

/* ------------------------------------------------------------------ */
/* Aufbau und Wechsel                                                  */
/* ------------------------------------------------------------------ */

static ui_state_t s_before_detail = UI_STATE_APPROVAL;

static void detail_close_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (s_gesture_in_press) return;
    ui_panel_show(s_before_detail);
}

static void open_detail_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (s_gesture_in_press) return;
    s_before_detail = s_current;
    ui_panel_show(UI_STATE_DETAIL);
}

/* Eine Geste wandert vom gedrueckten Objekt nach oben bis zum Screen
 * (LV_OBJ_FLAG_GESTURE_BUBBLE ist bei LVGL an jedem Kind gesetzt), deshalb
 * reicht ein Handler am Screen. */
static void gesture_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    s_gesture_in_press = true;      /* aus diesem Druck wird kein Tipp mehr */
    lv_indev_wait_release(indev);

    switch (lv_indev_get_gesture_dir(indev)) {
    case LV_DIR_BOTTOM:
        /* Erst in der Sessionliste zurueckblaettern. Steht sie schon oben,
         * ist die Geste frei fuer die Verbrauchsanzeige - so wie man es von
         * einer Liste erwartet, die man ueber den Anfang hinauszieht. */
        if (idle_scroll(-1)) break;
        if (s_current != UI_STATE_USAGE) {
            s_before_usage = s_current;
            ui_panel_show(UI_STATE_USAGE);
        }
        break;
    case LV_DIR_TOP:
        if (s_current == UI_STATE_USAGE) { ui_panel_show(s_before_usage); break; }
        idle_scroll(1);         /* weiter nach unten in der Liste */
        break;
    case LV_DIR_LEFT:
    case LV_DIR_RIGHT:
        /* Blaettern in der Warteschlange - beantwortet die Frage
         * "wer wartet denn noch", ohne dass man blind entscheiden muss. */
        if ((s_current == UI_STATE_APPROVAL || s_current == UI_STATE_QUEUE) && s_focus_cb)
            s_focus_cb(lv_indev_get_gesture_dir(indev) == LV_DIR_LEFT ? 1 : -1);
        break;
    default:
        break;
    }
}

void ui_panel_set_focus_cb(ui_focus_cb_t cb) { s_focus_cb = cb; }


void ui_panel_init(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(UI_C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < UI_STATE_COUNT; i++) {
        s_layer[i] = bare(scr, UI_W, UI_H);
        lv_obj_set_pos(s_layer[i], 0, 0);
        lv_obj_add_flag(s_layer[i], LV_OBJ_FLAG_HIDDEN);
    }

    build_idle(s_layer[UI_STATE_IDLE]);
    build_approval(s_layer[UI_STATE_APPROVAL]);
    build_queue(s_layer[UI_STATE_QUEUE]);
    build_detail(s_layer[UI_STATE_DETAIL]);
    build_usage(s_layer[UI_STATE_USAGE]);
    build_disconnected(s_layer[UI_STATE_DISCONNECTED]);

    lv_obj_add_event_cb(scr, gesture_cb, LV_EVENT_GESTURE, NULL);

    /* Ein Tipp auf die Kachel klappt den Kontext auf, ein Tipp im Detail
     * geht zurueck. */
    lv_obj_add_flag(s_layer[UI_STATE_DETAIL], LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_layer[UI_STATE_DETAIL], detail_close_cb, LV_EVENT_CLICKED, NULL);
    guard_taps(s_layer[UI_STATE_DETAIL]);

    request_view_t *views[2] = { &s_req, &s_req_q };
    for (int i = 0; i < 2; i++) {
        lv_obj_t *tile = lv_obj_get_parent(views[i]->agent);
        lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(tile, open_detail_cb, LV_EVENT_CLICKED, NULL);
        guard_taps(tile);
    }

    ui_panel_show(UI_STATE_IDLE);
}

void ui_panel_show(ui_state_t state)
{
    if (state < 0 || state >= UI_STATE_COUNT) return;
    for (int i = 0; i < UI_STATE_COUNT; i++) {
        if (i == (int)state) lv_obj_remove_flag(s_layer[i], LV_OBJ_FLAG_HIDDEN);
        else                 lv_obj_add_flag(s_layer[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (state == UI_STATE_IDLE && s_current != UI_STATE_IDLE) {
        s_idle_page = 0;
        idle_apply_page(false);
    }
    s_current = state;
}

ui_state_t ui_panel_current(void) { return s_current; }

void ui_panel_set_decision_cb(ui_decision_cb_t cb) { s_decision_cb = cb; }

/* ------------------------------------------------------------------ */
/* Inhalte                                                             */
/* ------------------------------------------------------------------ */

void ui_panel_set_idle_header(const char *left, const char *right)
{
    if (left)  lv_label_set_text(s_idle_left, left);
    if (right) {
        lv_label_set_text(s_idle_right, right);
        lv_obj_align(s_idle_right, LV_ALIGN_TOP_RIGHT, -UI_PAD_X, 14);
    }
}

void ui_panel_set_idle_row(int slot, bool active, const char *name, const char *age)
{
    if (slot < 0 || slot >= UI_IDLE_ROWS) return;

    /* Ohne Namen gibt es die Zeile nicht - vorher blieb hier ein Punkt ohne
     * Text stehen, was aussah, als liefe etwas Namenloses. */
    if (!name || !*name) {
        lv_obj_add_flag(s_idle_row[slot], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(s_idle_row[slot], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_idle_name[slot], name);
        lv_obj_set_style_text_color(s_idle_name[slot],
            lv_color_hex(active ? UI_C_TEXT_BRIGHT : UI_C_TEXT_GRAY), 0);
        lv_obj_set_style_bg_color(s_idle_dot[slot],
            lv_color_hex(active ? UI_C_GREEN : UI_C_DOT_IDLE), 0);
        lv_label_set_text(s_idle_age[slot], age ? age : "");
        lv_obj_align(s_idle_age[slot], LV_ALIGN_RIGHT_MID, 0, 0);
    }

    s_idle_count = 0;
    for (int i = 0; i < UI_IDLE_ROWS; i++)
        if (!lv_obj_has_flag(s_idle_row[i], LV_OBJ_FLAG_HIDDEN)) s_idle_count = i + 1;

    if (s_idle_count == 0) lv_obj_remove_flag(s_idle_stroll, LV_OBJ_FLAG_HIDDEN);
    else                   lv_obj_add_flag(s_idle_stroll, LV_OBJ_FLAG_HIDDEN);

    idle_apply_page(false);
}

static void apply_request(request_view_t *v, const char *agent,
                          const char *tool, const char *line1, const char *line2,
                          const char *meta)
{
    if (agent)    lv_label_set_text(v->agent, agent);
    if (tool)     lv_label_set_text(v->tool, tool);
    if (line1)    lv_label_set_text(v->line1, line1);
    lv_label_set_text(v->line2, line2 ? line2 : "");
    if (meta)     lv_label_set_text(v->meta, meta);
}

void ui_panel_set_request(const char *agent, const char *tool,
                          const char *line1, const char *line2, const char *meta)
{
    apply_request(&s_req, agent, tool, line1, line2, meta);
    if (meta) {
        strncpy(s_meta_base, meta, sizeof(s_meta_base) - 1);
        s_meta_base[sizeof(s_meta_base) - 1] = '\0';
    }
}

void ui_panel_set_queue(int position, int total, const char *next_agent)
{
    char buf[48];
    snprintf(buf, sizeof(buf), "%d of %d waiting", position, total);
    lv_label_set_text(s_req_q.status, buf);

    if (next_agent) {
        char meta[96];
        snprintf(meta, sizeof(meta), "a4f1 \xC2\xB7 4 s \xC2\xB7 then %s", next_agent);
        lv_label_set_text(s_req_q.meta, meta);
    }
    /* Die zweite Leiste faerbt sich nur, wenn wirklich mehr als einer wartet. */
    lv_obj_set_style_bg_color(s_queue_bar[1],
        lv_color_hex(total > 1 ? UI_C_QUEUE_IDLE : UI_C_BG), 0);
}

void ui_panel_set_detail(const char *header, const char *command,
                         const char *cwd, const char *consequence, const char *warning)
{
    if (header)      lv_label_set_text(s_detail_header, header);
    if (command)     lv_label_set_text(s_detail_cmd, command);
    if (cwd)         lv_label_set_text(s_detail_cwd, cwd);
    if (consequence) lv_label_set_text(s_detail_conseq, consequence);
    lv_label_set_text(s_detail_warn, warning ? warning : "");
}

void ui_panel_set_disconnected(const char *headline, const char *body, const char *meta)
{
    if (headline) lv_label_set_text(s_disc_head, headline);
    if (body)     lv_label_set_text(s_disc_body, body);
    if (meta)     lv_label_set_text(s_disc_meta, meta);
}

void ui_panel_set_usage(bool have, const char *model,
                        int five_pct,  const char *five_reset,
                        int week_pct,  const char *week_reset)
{
    if (model) lv_label_set_text(s_usage_model, model);
    lv_obj_align(s_usage_model, LV_ALIGN_TOP_RIGHT, -UI_PAD_X, 14);

    if (!have) {
        lv_obj_remove_flag(s_usage_none, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_usage_rows, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(s_usage_none, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_usage_rows, LV_OBJ_FLAG_HIDDEN);

    const int   pct[USAGE_ROWS]   = { five_pct,   week_pct   };
    const char *reset[USAGE_ROWS] = { five_reset, week_reset };

    for (int i = 0; i < USAGE_ROWS; i++) {
        char buf[16];
        if (pct[i] < 0) {
            /* Fenster fehlt - lieber ein Strich als eine erfundene Null. */
            lv_obj_set_width(s_bar_fill[i], 0);
            lv_label_set_text(s_bar_pct[i], "\xE2\x80\x94");
            lv_obj_set_style_text_color(s_bar_pct[i], lv_color_hex(UI_C_TEXT_FAINT), 0);
        } else {
            int p = pct[i] > 100 ? 100 : pct[i];
            lv_obj_set_width(s_bar_fill[i], BAR_W * p / 100);
            lv_obj_set_style_bg_color(s_bar_fill[i], lv_color_hex(load_color(p)), 0);
            snprintf(buf, sizeof(buf), "%d%%", pct[i]);
            lv_label_set_text(s_bar_pct[i], buf);
            lv_obj_set_style_text_color(s_bar_pct[i], lv_color_hex(UI_C_TEXT), 0);
        }
        lv_label_set_text(s_bar_reset[i], reset[i] ? reset[i] : "");
    }
}

void ui_panel_set_countdown(int seconds)
{
    char buf[96];
    s_countdown = seconds;
    snprintf(buf, sizeof(buf), "%s \xC2\xB7 %d s", s_meta_base, seconds);
    lv_label_set_text(s_req.meta, buf);
}

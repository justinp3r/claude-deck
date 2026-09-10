#include <Arduino.h>
#include <ArduinoJson.h>

#include "protocol.h"
#include "lvgl_port.h"
#include "ui_panel.h"

/* Grosszuegig: ein Bash-Befehl darf lang sein, und lieber verwerfen wir eine
 * zu lange Zeile sauber, als mitten im JSON abzuschneiden. */
#define LINE_MAX 3072

/* Selbsttest: laesst die Bridge einen Tastendruck simulieren ({"t":"tap","v":"allow"}).
 * MUSS im Betrieb 0 bleiben. Waere das an, koennte der Rechner sich selbst
 * freigeben - und genau das soll das Geraet ja verhindern. Nur zum Pruefen
 * der Kette ohne Finger am Display:
 *   arduino-cli compile --build-property build.extra_flags=-DALLOW_REMOTE_TAP=1 ...
 */
#ifndef ALLOW_REMOTE_TAP
#define ALLOW_REMOTE_TAP 0
#endif

static char     s_line[LINE_MAX];
static size_t   s_len = 0;
static bool     s_overflow = false;
static bool     s_host_seen = false;

static char     s_active_id[64] = "";
static int      s_ttl = 0;

bool protocol_host_seen(void) { return s_host_seen; }

static volatile bool          s_pending = false;
static volatile ui_decision_t s_pending_value = UI_DECISION_DENY;

/* Laeuft im LVGL-Task. Hier nur merken - geschrieben wird aus loop(), damit
 * Serial nicht aus zwei Tasks gleichzeitig beschrieben wird. */
static void on_decision(ui_decision_t d)
{
    s_pending_value = d;
    s_pending = true;
}

/* Laeuft in loop(). */
static void flush_decision(void)
{
    if (!s_pending) return;
    s_pending = false;

    /* Ohne offene Anfrage gibt es nichts zu beantworten - etwa wenn der
     * Countdown abgelaufen ist, waehrend der Finger unterwegs war. */
    if (s_active_id[0] == '\0') return;

    JsonDocument out;
    out["t"]  = "decision";
    out["id"] = s_active_id;
    out["v"]  = (s_pending_value == UI_DECISION_ALLOW) ? "allow" : "deny";
    serializeJson(out, Serial);
    Serial.println();

    s_active_id[0] = '\0';
    s_ttl = 0;
}

static void apply_request(JsonDocument &d)
{
    const char *id = d["id"] | "";
    strncpy(s_active_id, id, sizeof(s_active_id) - 1);
    s_active_id[sizeof(s_active_id) - 1] = '\0';
    s_ttl = d["ttl"] | 30;

    int qpos = d["qpos"] | 1;
    int qtot = d["qtot"] | 1;

    ui_panel_set_request(d["agent"] | "agent", d["tool"] | "Tool",
                         d["l1"] | "", d["l2"] | "", d["meta"] | "");
    ui_panel_set_detail(d["dhdr"] | "", d["full"] | "", d["cwd"] | "",
                        d["note"] | "", d["warn"] | "");

    if (qtot > 1) {
        ui_panel_set_queue(qpos, qtot, d["next"] | "");
        ui_panel_show(UI_STATE_QUEUE);
    } else {
        ui_panel_show(UI_STATE_APPROVAL);
    }
    ui_panel_set_countdown(s_ttl);
}

static void apply_usage(JsonDocument &d)
{
    ui_panel_set_usage(d["have"] | false,
                       d["h5"]  | -1, d["h5r"] | "",
                       d["wk"]  | -1, d["wkr"] | "");
}

/* Wischen nach links/rechts: die Bridge soll umblaettern. Laeuft im
 * LVGL-Task, deshalb hier nur merken - gesendet wird aus loop(). */
static volatile int s_focus_delta = 0;
static void on_focus(int delta) { s_focus_delta = delta; }

static void apply_idle(JsonDocument &d)
{
    ui_panel_set_idle_header(d["hdr"] | "", d["clock"] | "");
    JsonArray rows = d["rows"].as<JsonArray>();
    for (int i = 0; i < UI_IDLE_ROWS; i++) {
        if (!rows.isNull() && i < (int)rows.size()) {
            JsonObject r = rows[i];
            ui_panel_set_idle_row(i, r["on"] | false, r["n"] | "", r["a"] | "");
        } else {
            ui_panel_set_idle_row(i, false, "", "");
        }
    }

    /* Die Bridge schickt den Ruhezustand alle paar Sekunden erneut. Hat der
     * Nutzer gerade selbst etwas aufgeschlagen - Verbrauchsanzeige oder
     * Detailansicht -, waere ein Wechsel hierher ein Rausschmiss mitten im
     * Lesen. Die Daten werden trotzdem aktualisiert, nur die Ansicht bleibt.
     * Eine echte Freigabeanfrage darf weiterhin dazwischenfunken, die ist
     * dringend - das macht apply_request. */
    ui_state_t cur = ui_panel_current();
    if (cur != UI_STATE_USAGE && cur != UI_STATE_DETAIL) {
        ui_panel_show(UI_STATE_IDLE);
    }
}

static void handle_line(char *line)
{
    JsonDocument doc;
    if (deserializeJson(doc, line) != DeserializationError::Ok) return;

    const char *t = doc["t"] | "";
    if (!*t) return;

    s_host_seen = true;

    if (!strcmp(t, "ping")) { Serial.println("{\"t\":\"pong\"}"); return; }

    /* Alles Weitere fasst die UI an und muss durch das LVGL-Lock. */
    if (!lvgl_port_lock(200)) return;

    if (!strcmp(t, "req")) {
        apply_request(doc);
    } else if (!strcmp(t, "idle")) {
        s_active_id[0] = '\0';
        s_ttl = 0;
        apply_idle(doc);
    } else if (!strcmp(t, "usage")) {
        apply_usage(doc);
    } else if (!strcmp(t, "cancel")) {
        const char *id = doc["id"] | "";
        if (!*id || !strcmp(id, s_active_id)) {
            s_active_id[0] = '\0';
            s_ttl = 0;
            ui_panel_show(UI_STATE_IDLE);
        }
#if ALLOW_REMOTE_TAP
    } else if (!strcmp(t, "tap")) {
        const char *v = doc["v"] | "deny";
        on_decision(!strcmp(v, "allow") ? UI_DECISION_ALLOW : UI_DECISION_DENY);
#endif
    } else if (!strcmp(t, "link")) {
        bool up = doc["up"] | true;
        if (!up) {
            s_active_id[0] = '\0';
            ui_panel_show(UI_STATE_DISCONNECTED);
        }
    }

    lvgl_port_unlock();
}

void protocol_begin(void)
{
    if (lvgl_port_lock(-1)) {
        ui_panel_set_decision_cb(on_decision);
        ui_panel_set_focus_cb(on_focus);
        lvgl_port_unlock();
    }
    Serial.println("{\"t\":\"hello\"}");
}

static void flush_focus(void)
{
    if (!s_focus_delta) return;
    int d = s_focus_delta;
    s_focus_delta = 0;
    JsonDocument out;
    out["t"] = "focus";
    out["d"] = d;
    serializeJson(out, Serial);
    Serial.println();
}

/* Einzelne Tastendrucke im Monitor - JSON-Zeilen fangen immer mit '{' an,
 * deshalb laesst sich beides ohne Mehrdeutigkeit unterscheiden. */
static void handle_key(int c)
{
    if (c >= '1' && c <= '6') {
        static const ui_state_t order[6] = {
            UI_STATE_IDLE, UI_STATE_APPROVAL, UI_STATE_QUEUE,
            UI_STATE_DETAIL, UI_STATE_USAGE, UI_STATE_DISCONNECTED,
        };
        if (lvgl_port_lock(200)) { ui_panel_show(order[c - '1']); lvgl_port_unlock(); }
    } else if (c == '?') {
        Serial.println("1..6 = show state, ? = help");
    }
}

void protocol_poll(void)
{
    flush_decision();
    flush_focus();

    while (Serial.available()) {
        int c = Serial.read();
        /* Am Zeilenanfang und kein '{': das ist ein Tastendruck, keine Nachricht. */
        if (s_len == 0 && c != '{' && c != '\n' && c != '\r') { handle_key(c); continue; }
        if (c == '\n' || c == '\r') {
            if (s_len && !s_overflow) {
                s_line[s_len] = '\0';
                handle_line(s_line);
            }
            s_len = 0;
            s_overflow = false;
        } else if (s_len < LINE_MAX - 1) {
            s_line[s_len++] = (char)c;
        } else {
            /* Zeile zu lang: bis zum naechsten Umbruch verwerfen, statt
             * ein halbes Objekt zu parsen. */
            s_overflow = true;
        }
    }
}

void protocol_tick_second(void)
{
    if (s_active_id[0] == '\0' || s_ttl <= 0) return;

    s_ttl--;
    if (!lvgl_port_lock(100)) return;
    if (s_ttl > 0) {
        ui_panel_set_countdown(s_ttl);
    } else {
        /* Abgelaufen. Das Geraet entscheidet nicht von selbst - die Bridge
         * laeuft in denselben Timeout und gibt die Frage ans Terminal.
         * Wie bei apply_idle: wer gerade selbst etwas aufgeschlagen hat, wird
         * nicht herausgeworfen. */
        s_active_id[0] = '\0';
        ui_state_t cur = ui_panel_current();
        if (cur != UI_STATE_USAGE && cur != UI_STATE_DETAIL) {
            ui_panel_show(UI_STATE_IDLE);
        }
    }
    lvgl_port_unlock();
}

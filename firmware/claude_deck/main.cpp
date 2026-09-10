/* claude-deck - Firmware fuer ESP32-S3-Touch-LCD-3.49 V2.
 *
 * Warum hier und nicht in der .ino: Die .ino-Datei laeuft durch den
 * Arduino-Praeprozessor, der per ctags Funktionsprototypen einfuegt. Auf
 * Apple Silicon ist das mitgelieferte ctags ein x86-Binary und der Ersatz
 * verhaelt sich anders, wodurch die Prototypen falsch eingefuegt werden.
 * Als normales C++ uebersetzt, faellt das Problem weg.
 *
 * Ablauf: Solange sich die Bridge nicht gemeldet hat, laeuft eine Demo durch
 * die sechs Zustaende - damit ein frisch geflashtes Geraet ohne Mac etwas zeigt.
 * Sobald die erste Zeile ueber Serial kommt, uebernimmt das Protokoll.
 */

#include <Arduino.h>

#include "user_config.h"
#include "lvgl_port.h"
#include "i2c_bsp.h"
#include "src/lcd_bl_bsp/lcd_bl_pwm_bsp.h"
#include "ui_panel.h"
#include "protocol.h"

static const uint32_t DEMO_DWELL_MS = 4000;

static uint32_t s_last_switch = 0;
static uint32_t s_last_tick   = 0;
static int      s_demo_index  = 0;
static int      s_demo_count  = 12;

static const ui_state_t DEMO_ORDER[] = {
    UI_STATE_IDLE,
    UI_STATE_APPROVAL,
    UI_STATE_QUEUE,
    UI_STATE_DETAIL,
    UI_STATE_USAGE,
    UI_STATE_DISCONNECTED,
};
static const int DEMO_LEN = sizeof(DEMO_ORDER) / sizeof(DEMO_ORDER[0]);

void setup()
{
    /* Der USB-CDC-Empfangspuffer ist per Default nur 256 Byte gross, und
     * begin() setzt ihn nur, wenn er nicht schon vorgegeben ist. Eine
     * Freigabeanfrage mit vollem Befehl ist leicht 300+ Byte - die ginge
     * sonst still verloren, weil sie in einem Rutsch ankommt. Muss vor
     * begin() stehen. */
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);

    i2c_master_Init();
    lvgl_port_init();                        /* baut auch die UI auf */
    lcd_bl_pwm_bsp_init(LCD_PWM_MODE_255);   /* Backlight zuletzt */

    if (lvgl_port_lock(-1)) {
        ui_panel_set_queue(1, 2, "docs-sweep");
        lvgl_port_unlock();
    }
    protocol_begin();

    s_last_switch = millis();
    s_last_tick   = millis();
}

void loop()
{
    protocol_poll();

    uint32_t now = millis();

    if (now - s_last_tick >= 1000) {
        s_last_tick = now;

        if (protocol_host_seen()) {
            protocol_tick_second();
        } else if (ui_panel_current() == UI_STATE_APPROVAL) {
            /* Nur Deko fuer die Demo. */
            if (--s_demo_count < 0) s_demo_count = 12;
            if (lvgl_port_lock(100)) {
                ui_panel_set_countdown(s_demo_count);
                lvgl_port_unlock();
            }
        }
    }

    /* Die Demo laeuft nur, solange niemand zugehoert hat. */
    if (!protocol_host_seen() && now - s_last_switch >= DEMO_DWELL_MS) {
        s_demo_index = (s_demo_index + 1) % DEMO_LEN;
        if (lvgl_port_lock(100)) {
            ui_panel_show(DEMO_ORDER[s_demo_index]);
            lvgl_port_unlock();
        }
        s_last_switch = now;
    }

    delay(5);
}

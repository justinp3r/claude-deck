/* Firmware fuer ESP32-S3-Touch-LCD-3.49 V2. Warum main.cpp und nicht die .ino:
 * siehe claude_deck.ino. */

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
    /* Muss vor begin() stehen: Default sind 256 Byte, eine Anfrage ist groesser
     * und kommt in einem Rutsch - sie ginge sonst still verloren. */
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

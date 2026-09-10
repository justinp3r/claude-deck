#ifndef LVGL_PORT_H
#define LVGL_PORT_H



#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>

/* Initialisiert Expander, QSPI, AXS15231B, Backlight, LVGL, Touch und startet
 * den LVGL-Task. Baut danach die UI ueber ui_panel_init() auf. */
void lvgl_port_init(void);

/* LVGL laeuft in einem eigenen Task. Vor jedem lv_*-Aufruf von aussen locken.
 * timeout_ms = -1 wartet unbegrenzt. */
bool lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);


#ifdef __cplusplus
}
#endif



#endif











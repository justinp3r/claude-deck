#ifndef LVGL_PORT_H
#define LVGL_PORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/* Initialisiert Display, Touch und LVGL und baut die UI auf. */
void lvgl_port_init(void);

/* Vor jedem lv_*-Aufruf von aussen locken. timeout_ms = -1 wartet unbegrenzt. */
bool lvgl_port_lock(int timeout_ms);
void lvgl_port_unlock(void);

#ifdef __cplusplus
}
#endif

#endif


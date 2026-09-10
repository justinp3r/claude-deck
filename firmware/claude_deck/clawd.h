/* Clawd, aus neun Rechtecken statt als Bitmap - so laeuft jedes Bein einzeln. */
#ifndef CLAWD_H
#define CLAWD_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Legt Clawd zentriert in parent an und startet die Animationen. */
lv_obj_t *clawd_create(lv_obj_t *parent, int width);

/* Hoehe, die clawd_create bei dieser Breite belegt (inkl. Beinspielraum). */
int clawd_height(int width);

#ifdef __cplusplus
}
#endif

#endif

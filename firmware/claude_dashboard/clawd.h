/* Clawd - das Claude-Code-Maskottchen, animiert.
 *
 * Nicht als Bitmap, sondern aus Rechtecken gebaut. Die Vorlage
 * (SquareLineStudioExport/images/ui_img_clawd_png.c) ist Pixel-Art aus genau
 * neun Rechtecken - als Bild waeren es 161 KB, die sich nur als Ganzes bewegen
 * liessen. So kostet es nichts und jedes Bein laeuft einzeln.
 */
#ifndef CLAWD_H
#define CLAWD_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Legt Clawd in parent an, zentriert auf der angegebenen Breite, und startet
 * die Animationen (wippen, laufen, blinzeln). Gibt den Container zurueck. */
lv_obj_t *clawd_create(lv_obj_t *parent, int width);

/* Hoehe, die clawd_create bei dieser Breite belegt (inkl. Beinspielraum). */
int clawd_height(int width);


#ifdef __cplusplus
}
#endif

#endif

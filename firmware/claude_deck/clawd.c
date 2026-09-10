#include "clawd.h"
#include "ui_theme.h"

/* Masse aus der Vorlage, bezogen auf die Bounding-Box (378 x 253). */
#define SRC_W 378
#define SRC_H 253

#define TORSO_X   60
#define TORSO_W  259
#define TORSO_H  185      /* bis dort, wo die Beine anfangen */

#define ARM_Y     61
#define ARM_H     64
#define ARM_L_W   60
#define ARM_R_X  319
#define ARM_R_W   59

#define EYE_Y     28
#define EYE_W     34
#define EYE_H     33
#define EYE_L_X   95
#define EYE_R_X  249

#define LEG_Y    185
#define LEG_H     68

static const int LEG_X[4] = {  60, 124, 219, 282 };
static const int LEG_W[4] = {  35,  34,  33,  37 };

#define SX(v, w) (((v) * (w) + SRC_W / 2) / SRC_W)

typedef struct {
    lv_obj_t *body;
    lv_obj_t *eye[2];
    int       eye_y, eye_h;
} clawd_t;

static void on_delete(lv_event_t *e)
{
    clawd_t *c = (clawd_t *)lv_event_get_user_data(e);
    lv_free(c);
}

/* Beide Augen gemeinsam zukneifen. v: 0 = offen, 100 = zu. */
static void blink_exec(void *var, int32_t v)
{
    clawd_t *c = (clawd_t *)lv_obj_get_user_data((lv_obj_t *)var);
    if (!c) return;
    int h = c->eye_h - (c->eye_h - 2) * v / 100;
    if (h < 1) h = 1;
    int y = c->eye_y + (c->eye_h - h) / 2;    /* mittig zusammenziehen */
    for (int i = 0; i < 2; i++) {
        lv_obj_set_height(c->eye[i], h);
        lv_obj_set_y(c->eye[i], y);
    }
}

static void bob_start(clawd_t *c);

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    /* lv_obj_create setzt LV_OBJ_FLAG_CLICKABLE von sich aus - sonst faengt jedes
     * Deko-Rechteck die Beruehrung ab, die dem Elternteil galt. */
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return o;
}

int clawd_height(int width)
{
    /* +3 px Spielraum, damit die laufenden Beine unten nicht abgeschnitten werden */
    return SX(SRC_H, width) + 3;
}

lv_obj_t *clawd_create(lv_obj_t *parent, int width)
{
    const int W = width;
    const int H = clawd_height(width);

    /* root wird positioniert, body wippt - getrennt, damit sich beides nicht stoert. */
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    /* Nicht klickbar: der Tipp gilt der Flaeche dahinter. */
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(root, W, H);

    lv_obj_t *body = lv_obj_create(root);
    lv_obj_remove_style_all(body);
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(body, 0, 0);
    lv_obj_set_size(body, W, H);

    clawd_t *c = (clawd_t *)lv_malloc(sizeof(clawd_t));
    LV_ASSERT_MALLOC(c);
    lv_memzero(c, sizeof(clawd_t));
    c->body = body;
    lv_obj_set_user_data(body, c);
    lv_obj_add_event_cb(body, on_delete, LV_EVENT_DELETE, c);

    const int leg_y = SX(LEG_Y, W);
    const int leg_h = SX(LEG_H, W);

    block(body, SX(TORSO_X, W), 0, SX(TORSO_W, W), SX(TORSO_H, W), UI_C_ACCENT);
    block(body, 0, SX(ARM_Y, W), SX(ARM_L_W, W), SX(ARM_H, W), UI_C_ACCENT);
    block(body, SX(ARM_R_X, W), SX(ARM_Y, W), SX(ARM_R_W, W), SX(ARM_H, W), UI_C_ACCENT);

    /* Vier Beine, zwei davon gegenphasig - das liest sich als Schritt. */
    for (int i = 0; i < 4; i++) {
        lv_obj_t *leg = block(body, SX(LEG_X[i], W), leg_y, SX(LEG_W[i], W), leg_h, UI_C_ACCENT);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, leg);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_height);
        lv_anim_set_duration(&a, 420);
        lv_anim_set_playback_duration(&a, 420);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        if (i % 2 == 0) lv_anim_set_values(&a, leg_h, leg_h + 3);
        else            lv_anim_set_values(&a, leg_h + 3, leg_h);
        lv_anim_start(&a);
    }

    /* Augen als Loecher in der Kachelfarbe, nicht als schwarze Flecken */
    c->eye_y = SX(EYE_Y, W);
    c->eye_h = SX(EYE_H, W);
    c->eye[0] = block(body, SX(EYE_L_X, W), c->eye_y, SX(EYE_W, W), c->eye_h, UI_C_TILE);
    c->eye[1] = block(body, SX(EYE_R_X, W), c->eye_y, SX(EYE_W, W), c->eye_h, UI_C_TILE);

    /* Blinzeln: kurz und selten, sonst wird es zappelig. */
    lv_anim_t b;
    lv_anim_init(&b);
    lv_anim_set_var(&b, body);
    lv_anim_set_exec_cb(&b, blink_exec);
    lv_anim_set_values(&b, 0, 100);
    lv_anim_set_duration(&b, 70);
    lv_anim_set_playback_duration(&b, 90);
    lv_anim_set_repeat_delay(&b, 3400);
    lv_anim_set_repeat_count(&b, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&b);

    /* Wippen laeuft auf body, unabhaengig davon, wohin root spaeter geschoben wird. */
    bob_start(c);

    return root;
}

/* --- Wippen --- */

static void bob_start(clawd_t *c)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, c->body);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_set_values(&a, 0, -2);
    lv_anim_set_duration(&a, 840);
    lv_anim_set_playback_duration(&a, 840);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

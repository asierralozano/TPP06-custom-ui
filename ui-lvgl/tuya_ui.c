#include "tuya_ui.h"

/* Utilidades compartidas de la UI Tuya. De momento: un único callback de
 * "volver" que el router (ui_nav) fija una vez, y al que se enganchan todas las
 * flechas de volver de las cabeceras (detalles, ajustes, escenas, wizard). */

static void (*s_back_cb)(void) = 0;

void tuya_ui_set_back_cb(void (*cb)(void)){ s_back_cb = cb; }

static void back_clicked(lv_event_t *e){
    (void)e;
    if(s_back_cb) s_back_cb();
}

void tuya_ui_attach_back(lv_obj_t *back_img){
    if(!back_img) return;
    /* Área de toque cómoda alrededor del icono (44x44) y clicable. */
    lv_obj_add_flag(back_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(back_img, 16);
    lv_obj_add_event_cb(back_img, back_clicked, LV_EVENT_CLICKED, 0);
}

#include "tuya_screensaver.h"
#include "fonts.h"

/* Salvapantallas estilo Tuya — screen_saver_1 (de theme/screensaver.lua, 1:1).
 * Dos cajas redondeadas translúcidas apiladas: HH arriba, MM abajo, con una
 * línea negra cruzando el centro de cada una (efecto flip-clock). Fuente Manrope
 * 150 (weight 800 en Tuya; aquí ~400 hasta instanciar la estática con fonttools). */
static void saver_box(lv_obj_t *scr, int y, const char *txt){
    /* Caja: 212x206, radio 16, blanca al ~10% de opacidad */
    lv_obj_t *bg = lv_obj_create(scr);
    lv_obj_set_size(bg, 212, 206);
    lv_obj_align(bg, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(bg, 16, 0);
    lv_obj_set_style_bg_color(bg, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(bg, 26, 0);            /* OPA_10 ~ 10% */
    lv_obj_set_style_border_width(bg, 0, 0);
    lv_obj_set_style_pad_all(bg, 0, 0);
    lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);

    /* Número grande centrado, blanco al ~80% */
    lv_obj_t *num = lv_label_create(scr);
    lv_label_set_text(num, txt);
    lv_obj_set_style_text_font(num, manrope(150), 0);
    lv_obj_set_style_text_color(num, lv_color_white(), 0);
    lv_obj_set_style_text_opa(num, 204, 0);        /* TX_OPA_80 */
    lv_obj_set_width(num, 240);
    lv_obj_set_style_text_align(num, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(num, LV_ALIGN_TOP_MID, 0, y - 5);

    /* Línea negra cruzando el centro (212x4) */
    lv_obj_t *line = lv_obj_create(scr);
    lv_obj_set_size(line, 212, 4);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, y + 106);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_bg_color(line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
}

void tuya_screensaver_build(lv_obj_t *scr){
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    saver_box(scr, 25, "06");    /* horas */
    saver_box(scr, 247, "30");   /* minutos */
}

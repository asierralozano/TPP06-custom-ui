#include "tuya_home.h"
#include "fonts.h"
#include "assets.h"

/* Home estilo Tuya — pantalla 0 (reloj/escritorio estilo 0 de theme/desktop.lua).
 * Coordenadas 1:1 del firmware. Formato 24h (time_font_size=90).
 * Usa el wallpaper real de Tuya (theme/bg/00.png) y el icono de clima (weather/36). */
void tuya_home_build(lv_obj_t *scr){
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Wallpaper a pantalla completa (fondo de Tuya) */
    lv_obj_t *bg = lv_img_create(scr);
    lv_img_set_src(bg, asset("theme/bg/00.png"));
    lv_obj_align(bg, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Icono de clima 36px, centrado en caja 74x64 @ (5,15) -> esquina (24,29) */
    lv_obj_t *wicon = lv_img_create(scr);
    lv_img_set_src(wicon, asset("weather/36/sunny.png"));
    lv_obj_align(wicon, LV_ALIGN_TOP_LEFT, 24, 29);

    /* Temperatura: Manrope 19, blanco, TOP_LEFT (68,37) */
    lv_obj_t *temp = lv_label_create(scr);
    lv_label_set_text(temp, "26°");
    lv_obj_set_style_text_font(temp, manrope(19), 0);
    lv_obj_set_style_text_color(temp, lv_color_white(), 0);
    lv_obj_align(temp, LV_ALIGN_TOP_LEFT, 68, 37);

    /* Fecha: Manrope 16, blanco, TOP_LEFT (30,66) */
    lv_obj_t *day = lv_label_create(scr);
    lv_label_set_text(day, "lun 24 jun");
    lv_obj_set_style_text_font(day, manrope(16), 0);
    lv_obj_set_style_text_color(day, lv_color_white(), 0);
    lv_obj_align(day, LV_ALIGN_TOP_LEFT, 30, 66);

    /* Hora grande: Manrope 90 (24h), blanco, centrada, ancho de pantalla, y=95 */
    lv_obj_t *t = lv_label_create(scr);
    lv_label_set_text(t, "06:30");
    lv_obj_set_style_text_font(t, manrope(90), 0);
    lv_obj_set_style_text_color(t, lv_color_white(), 0);
    lv_obj_set_width(t, 320);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 95);
}

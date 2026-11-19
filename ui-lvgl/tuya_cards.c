#include "tuya_cards.h"
#include "fonts.h"
#include "assets.h"

/* Tarjeta de dispositivo estilo Tuya. Marco translúcido + botón on/off (icon/on|off.png)
 * + nombre + estado + slider de brillo opcional. (Dimensiones a afinar con screenshot
 * del panel; internos basados en gui_cont_page/cont_light.lua.) */
lv_obj_t *tuya_card(lv_obj_t *parent, int x, int y, int w,
                    const char *name, const char *sub, int on, int bright){
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, 96);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x14141A), 0);
    lv_obj_set_style_bg_opa(card, 185, 0);                 /* translúcido sobre el wallpaper */
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Botón switch: icon/on.png (verde) u off.png (gris), 56x56 */
    lv_obj_t *sw = lv_img_create(card);
    lv_img_set_src(sw, asset(on ? "icon/on.png" : "icon/off.png"));
    lv_obj_align(sw, LV_ALIGN_TOP_LEFT, 14, 20);

    /* Nombre */
    lv_obj_t *nm = lv_label_create(card);
    lv_label_set_text(nm, name);
    lv_obj_set_style_text_font(nm, manrope(17), 0);
    lv_obj_set_style_text_color(nm, lv_color_white(), 0);
    lv_obj_align(nm, LV_ALIGN_TOP_LEFT, 84, 18);

    /* Estado / subtítulo */
    lv_obj_t *st = lv_label_create(card);
    lv_label_set_text(st, sub);
    lv_obj_set_style_text_font(st, manrope(14), 0);
    lv_obj_set_style_text_color(st, on ? lv_color_hex(0x9AE6C0) : lv_color_hex(0x8A8A95), 0);
    lv_obj_align(st, LV_ALIGN_TOP_LEFT, 84, 46);

    /* Slider de brillo (solo si aplica) */
    if(bright >= 0){
        lv_obj_t *bar = lv_bar_create(card);
        lv_obj_set_size(bar, w - 84 - 16, 8);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 84, 72);
        lv_obj_set_style_radius(bar, 4, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x33333A), 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x57C891), LV_PART_INDICATOR);
        lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
        lv_bar_set_value(bar, bright, LV_ANIM_OFF);
    }
    return card;
}

/* Tarjeta de SENSOR (temp/humedad) estilo Tuya (de cont_wsdcg.lua): nombre arriba,
 * dos valores grandes lado a lado (Manrope 36/500) con etiqueta debajo (22/400). */
lv_obj_t *tuya_card_sensor(lv_obj_t *parent, int x, int y, int w,
                           const char *name, const char *temp, const char *humi){
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, 150);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x14141A), 0);
    lv_obj_set_style_bg_opa(card, 185, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *nm = lv_label_create(card);
    lv_label_set_text(nm, name);
    lv_obj_set_style_text_font(nm, manrope(17), 0);
    lv_obj_set_style_text_color(nm, lv_color_white(), 0);
    lv_obj_align(nm, LV_ALIGN_TOP_LEFT, 18, 16);

    int half = w / 2;
    /* Temperatura (izquierda) */
    lv_obj_t *tv = lv_label_create(card);
    lv_label_set_text(tv, temp);
    lv_obj_set_style_text_font(tv, manrope(36), 0);
    lv_obj_set_style_text_color(tv, lv_color_white(), 0);
    lv_obj_set_width(tv, half);
    lv_obj_set_style_text_align(tv, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tv, LV_ALIGN_TOP_LEFT, 0, 60);

    lv_obj_t *tl = lv_label_create(card);
    lv_label_set_text(tl, "Temperatura");
    lv_obj_set_style_text_font(tl, manrope(15), 0);
    lv_obj_set_style_text_color(tl, lv_color_hex(0x8A8A95), 0);
    lv_obj_set_width(tl, half);
    lv_obj_set_style_text_align(tl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tl, LV_ALIGN_TOP_LEFT, 0, 112);

    /* Humedad (derecha) */
    lv_obj_t *hv = lv_label_create(card);
    lv_label_set_text(hv, humi);
    lv_obj_set_style_text_font(hv, manrope(36), 0);
    lv_obj_set_style_text_color(hv, lv_color_white(), 0);
    lv_obj_set_width(hv, half);
    lv_obj_set_style_text_align(hv, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hv, LV_ALIGN_TOP_LEFT, half, 60);

    lv_obj_t *hl = lv_label_create(card);
    lv_label_set_text(hl, "Humedad");
    lv_obj_set_style_text_font(hl, manrope(15), 0);
    lv_obj_set_style_text_color(hl, lv_color_hex(0x8A8A95), 0);
    lv_obj_set_width(hl, half);
    lv_obj_set_style_text_align(hl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hl, LV_ALIGN_TOP_LEFT, half, 112);
    return card;
}

/* Página de dispositivos (demo): wallpaper + título de estancia + tarjetas. */
void tuya_devices_build(lv_obj_t *scr){
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *bg = lv_img_create(scr);
    lv_img_set_src(bg, asset("theme/bg/00.png"));
    lv_obj_align(bg, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Título de estancia */
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Salon");
    lv_obj_set_style_text_font(title, manrope(26), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 22);

    /* Tarjetas */
    tuya_card(scr, 16, 70,  288, "Mesa",    "Encendida",  1, 80);
    tuya_card(scr, 16, 178, 288, "Techo",   "Apagada",    0, -1);
    tuya_card(scr, 16, 286, 288, "Enchufe", "Encendido",  1, -1);
}

/* Página "catálogo" (demo): muestra los distintos tipos de tarjeta a la vez
 * (sensor temp/humedad + luz con brillo + interruptor) sobre el wallpaper. */
void tuya_devices_mix_build(lv_obj_t *scr){
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *bg = lv_img_create(scr);
    lv_img_set_src(bg, asset("theme/bg/00.png"));
    lv_obj_align(bg, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Cocina");
    lv_obj_set_style_text_font(title, manrope(26), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 22);

    tuya_card_sensor(scr, 16, 70, 288, "Clima", "22 °C", "48 %");
    tuya_card(scr, 16, 236, 288, "Lampara", "Encendida", 1, 65);
    tuya_card(scr, 16, 344, 288, "Cafetera", "Apagada",   0, -1);
}

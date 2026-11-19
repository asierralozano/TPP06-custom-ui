#include "tuya_control_center.h"
#include "fonts.h"
#include "assets.h"

/* Centro de control estilo Tuya (desplegable al deslizar desde arriba):
 * tirador + slider de brillo grande + tiles de acción rápida + barra inferior
 * con "Ajustes" (icon/menu_setting.png) y "Editar" (icon/edit.png).
 * El centro de control nativo de Tuya lo dibuja su motor C (no hay Lua), así que
 * se reconstruye con sus iconos reales manteniendo la estética. */

/* Tile de acción rápida: cuadro redondeado 136x82 con icono + etiqueta. */
static void quick_tile(lv_obj_t *scr, int x, int y, const char *icon,
                       const char *label, int active){
    lv_obj_t *t = lv_obj_create(scr);
    lv_obj_set_size(t, 136, 82);
    lv_obj_set_pos(t, x, y);
    lv_obj_set_style_radius(t, 18, 0);
    lv_obj_set_style_bg_color(t, active ? lv_color_hex(0x1E3A30) : lv_color_hex(0x16161C), 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(t, 0, 0);
    lv_obj_set_style_pad_all(t, 0, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ic = lv_img_create(t);
    lv_img_set_src(ic, asset(icon));
    lv_obj_set_style_img_recolor(ic, active ? lv_color_hex(0x57C891) : lv_color_hex(0xB8B8C0), 0);
    lv_obj_set_style_img_recolor_opa(ic, LV_OPA_COVER, 0);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 16, 14);

    lv_obj_t *lb = lv_label_create(t);
    lv_label_set_text(lb, label);
    lv_obj_set_style_text_font(lb, manrope(15), 0);
    lv_obj_set_style_text_color(lb, lv_color_white(), 0);
    lv_obj_align(lb, LV_ALIGN_BOTTOM_LEFT, 16, -14);
}

void tuya_control_center_build(lv_obj_t *scr){
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x08080C), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Tirador superior */
    lv_obj_t *handle = lv_obj_create(scr);
    lv_obj_set_size(handle, 44, 5);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_set_style_radius(handle, 3, 0);
    lv_obj_set_style_bg_color(handle, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(handle, 90, 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_clear_flag(handle, LV_OBJ_FLAG_SCROLLABLE);

    /* Slider de brillo grande (barra redondeada con icono dentro) */
    lv_obj_t *bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 288, 64);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 36);
    lv_obj_set_style_radius(bar, 24, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x16161C), 0);
    lv_obj_set_style_radius(bar, 24, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x3A3A46), LV_PART_INDICATOR);
    lv_bar_set_range(bar, 1, 100);
    lv_bar_set_value(bar, 72, LV_ANIM_OFF);

    lv_obj_t *bic = lv_img_create(scr);
    lv_img_set_src(bic, asset("icon/brightness.png"));
    lv_obj_set_style_img_recolor(bic, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(bic, LV_OPA_COVER, 0);
    lv_obj_align(bic, LV_ALIGN_TOP_LEFT, 32, 36 + 11);

    /* Tiles de acción rápida (2x2) */
    quick_tile(scr, 16,  120, "icon/on.png",           "Todo encendido", 1);
    quick_tile(scr, 168, 120, "icon/off.png",          "Todo apagado",   0);
    quick_tile(scr, 16,  214, "icon/menu_setting.png", "Modo noche",     0);
    quick_tile(scr, 168, 214, "icon/network.png",      "WiFi",           1);

    /* Barra inferior: Ajustes | Editar */
    lv_obj_t *sbar = lv_obj_create(scr);
    lv_obj_set_size(sbar, 288, 72);
    lv_obj_align(sbar, LV_ALIGN_TOP_MID, 0, 320);
    lv_obj_set_style_radius(sbar, 20, 0);
    lv_obj_set_style_bg_color(sbar, lv_color_hex(0x16161C), 0);
    lv_obj_set_style_bg_opa(sbar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sbar, 0, 0);
    lv_obj_set_style_pad_all(sbar, 0, 0);
    lv_obj_clear_flag(sbar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *set_ic = lv_img_create(sbar);
    lv_img_set_src(set_ic, asset("icon/menu_setting.png"));
    lv_obj_set_style_img_recolor(set_ic, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(set_ic, LV_OPA_COVER, 0);
    lv_obj_align(set_ic, LV_ALIGN_LEFT_MID, 36, 0);
    lv_obj_t *set_lb = lv_label_create(sbar);
    lv_label_set_text(set_lb, "Ajustes");
    lv_obj_set_style_text_font(set_lb, manrope(16), 0);
    lv_obj_set_style_text_color(set_lb, lv_color_white(), 0);
    lv_obj_align(set_lb, LV_ALIGN_LEFT_MID, 100, 0);

    /* Separador vertical */
    lv_obj_t *sep = lv_obj_create(sbar);
    lv_obj_set_size(sep, 2, 40);
    lv_obj_align(sep, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(sep, lv_color_hex(0x33333A), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ed_ic = lv_img_create(sbar);
    lv_img_set_src(ed_ic, asset("icon/edit.png"));
    lv_obj_set_style_img_recolor(ed_ic, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(ed_ic, LV_OPA_COVER, 0);
    lv_obj_align(ed_ic, LV_ALIGN_RIGHT_MID, -96, 0);
    lv_obj_t *ed_lb = lv_label_create(sbar);
    lv_label_set_text(ed_lb, "Editar");
    lv_obj_set_style_text_font(ed_lb, manrope(16), 0);
    lv_obj_set_style_text_color(ed_lb, lv_color_white(), 0);
    lv_obj_align(ed_lb, LV_ALIGN_RIGHT_MID, -36, 0);
}

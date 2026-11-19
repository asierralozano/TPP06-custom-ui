#include "tuya_scenes.h"
#include "fonts.h"
#include "assets.h"
#include "tuya_ui.h"

/* Página de Escenas estilo Tuya: rejilla de tiles (badge circular con icono de
 * escena + nombre). Las escenas son de Home Assistant; iconos reales del
 * firmware (menu_home / menu_leave_home / menu_in_home / shortcut_add). */

/* Tile de escena: 140x126, badge circular 64 con icono + nombre debajo. */
static void scene_tile(lv_obj_t *scr, int x, int y, const char *icon,
                       uint32_t accent, const char *name, int add){
    lv_obj_t *t = lv_obj_create(scr);
    lv_obj_set_size(t, 140, 126);
    lv_obj_set_pos(t, x, y);
    lv_obj_set_style_radius(t, 20, 0);
    lv_obj_set_style_bg_color(t, lv_color_hex(0x14141A), 0);
    lv_obj_set_style_bg_opa(t, 220, 0);
    lv_obj_set_style_border_width(t, add ? 2 : 0, 0);
    lv_obj_set_style_border_color(t, lv_color_hex(0x33333A), 0);
    lv_obj_set_style_border_opa(t, add ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(t, 0, 0);
    lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *badge = lv_obj_create(t);
    lv_obj_set_size(badge, 64, 64);
    lv_obj_align(badge, LV_ALIGN_TOP_MID, 0, 16);
    lv_obj_set_style_radius(badge, 32, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(accent), 0);
    lv_obj_set_style_bg_opa(badge, add ? 40 : LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ic = lv_img_create(badge);
    lv_img_set_src(ic, asset(icon));
    lv_img_set_zoom(ic, 160);                 /* ~56/50 → 40px aprox */
    lv_obj_set_style_img_recolor(ic, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(ic, LV_OPA_COVER, 0);
    lv_obj_center(ic);

    lv_obj_t *nm = lv_label_create(t);
    lv_label_set_text(nm, name);
    lv_obj_set_style_text_font(nm, manrope(16), 0);
    lv_obj_set_style_text_color(nm, lv_color_white(), 0);
    lv_obj_set_width(nm, 132);
    lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(nm, LV_ALIGN_BOTTOM_MID, 0, -14);
}

void tuya_scenes_build(lv_obj_t *scr){
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0C0C10), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Header */
    lv_obj_t *back = lv_img_create(scr);
    lv_img_set_src(back, asset("icon/left.png"));
    lv_obj_set_style_img_recolor(back, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(back, LV_OPA_COVER, 0);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 20, 22);
    tuya_ui_attach_back(back);

    lv_obj_t *ttl = lv_label_create(scr);
    lv_label_set_text(ttl, "Escenas");
    lv_obj_set_style_text_font(ttl, manrope(22), 0);
    lv_obj_set_style_text_color(ttl, lv_color_white(), 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 58, 20);

    /* Rejilla 2x2 + tile de añadir */
    scene_tile(scr, 16,  78,  "icon/menu_in_home.png",    0xF0A24F, "Buenos dias",  0);
    scene_tile(scr, 164, 78,  "icon/menu_leave_home.png", 0x5A8DEE, "Salir",        0);
    scene_tile(scr, 16,  216, "icon/menu_home.png",       0x57C891, "En casa",      0);
    scene_tile(scr, 164, 216, "icon/menu_shortcut.png",   0xB06FE0, "Noche",        0);
    scene_tile(scr, 16,  354, "icon/shortcut_add.png",    0xFFFFFF, "Anadir",       1);
}

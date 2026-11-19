#include "tuya_add_entity.h"
#include "fonts.h"
#include "assets.h"
#include "tuya_ui.h"

/* Wizard "Añadir entidad" estilo Tuya: header + chips de dominio + lista de
 * entidades de Home Assistant (badge con icono del dominio + nombre + dominio +
 * botón añadir icon/cont_add.png). Es la versión restyle del flujo de añadir que
 * ya teníamos en nuestra UI, con la estética del panel Tuya. */

static int s_row_y;

/* Chip de filtro de dominio (redondeado). active = relleno verde. */
static void chip(lv_obj_t *scr, int x, const char *txt, int active){
    lv_obj_t *c = lv_obj_create(scr);
    lv_obj_set_size(c, LV_SIZE_CONTENT, 34);
    lv_obj_set_pos(c, x, 70);
    lv_obj_set_style_radius(c, 17, 0);
    lv_obj_set_style_bg_color(c, active ? lv_color_hex(0x57C891) : lv_color_hex(0x1A1A20), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_hor(c, 16, 0);
    lv_obj_set_style_pad_ver(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(c);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, manrope(14), 0);
    lv_obj_set_style_text_color(l, active ? lv_color_hex(0x06281C) : lv_color_hex(0xB8B8C0), 0);
    lv_obj_center(l);
}

/* Fila de entidad: badge+icono (zoom a ~30px) + nombre + dominio + botón añadir.
 * recolor: color del icono, o 0 para dejar el color nativo del asset. */
static void entity_row(lv_obj_t *scr, const char *icon, int zoom, uint32_t recolor,
                       const char *name, const char *domain){
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_set_size(row, 288, 64);
    lv_obj_set_pos(row, 16, s_row_y);
    s_row_y += 72;
    lv_obj_set_style_radius(row, 16, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x14141A), 0);
    lv_obj_set_style_bg_opa(row, 230, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Badge circular */
    lv_obj_t *badge = lv_obj_create(row);
    lv_obj_set_size(badge, 46, 46);
    lv_obj_align(badge, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_radius(badge, 23, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x22222A), 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ic = lv_img_create(badge);
    lv_img_set_src(ic, asset(icon));
    lv_img_set_zoom(ic, zoom);
    if(recolor){
        lv_obj_set_style_img_recolor(ic, lv_color_hex(recolor), 0);
        lv_obj_set_style_img_recolor_opa(ic, LV_OPA_COVER, 0);
    }
    lv_obj_center(ic);

    /* Nombre */
    lv_obj_t *nm = lv_label_create(row);
    lv_label_set_text(nm, name);
    lv_obj_set_style_text_font(nm, manrope(17), 0);
    lv_obj_set_style_text_color(nm, lv_color_white(), 0);
    lv_obj_align(nm, LV_ALIGN_LEFT_MID, 70, -10);

    /* Dominio (gris) */
    lv_obj_t *dm = lv_label_create(row);
    lv_label_set_text(dm, domain);
    lv_obj_set_style_text_font(dm, manrope(13), 0);
    lv_obj_set_style_text_color(dm, lv_color_hex(0x8A8A95), 0);
    lv_obj_align(dm, LV_ALIGN_LEFT_MID, 70, 12);

    /* Botón añadir (icon/cont_add.png) */
    lv_obj_t *add = lv_img_create(row);
    lv_img_set_src(add, asset("icon/cont_add.png"));
    lv_obj_align(add, LV_ALIGN_RIGHT_MID, -18, 0);
}

void tuya_add_entity_build(lv_obj_t *scr){
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0C0C10), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Header: volver + título + buscar */
    lv_obj_t *back = lv_img_create(scr);
    lv_img_set_src(back, asset("icon/left.png"));
    lv_obj_set_style_img_recolor(back, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(back, LV_OPA_COVER, 0);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 20, 22);
    tuya_ui_attach_back(back);

    lv_obj_t *ttl = lv_label_create(scr);
    lv_label_set_text(ttl, "Anadir entidad");
    lv_obj_set_style_text_font(ttl, manrope(22), 0);
    lv_obj_set_style_text_color(ttl, lv_color_white(), 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 58, 20);

    lv_obj_t *srch = lv_img_create(scr);
    lv_img_set_src(srch, asset("icon/search.png"));
    lv_obj_set_style_img_recolor(srch, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(srch, LV_OPA_COVER, 0);
    lv_obj_align(srch, LV_ALIGN_TOP_RIGHT, -22, 26);

    /* Chips de dominio */
    chip(scr, 16,  "Luces",        1);
    chip(scr, 92,  "Interruptores",0);
    chip(scr, 228, "Covers",       0);

    /* Lista de entidades de HA (demo). zoom normaliza el icono a ~30px. */
    s_row_y = 120;
    entity_row(scr, "icon/light_rectangle.png",   154, 0xFFC24B, "Lampara salon",     "light.lampara_salon");
    entity_row(scr, "icon/relay_switch.png",      120, 0,        "Enchufe TV",        "switch.enchufe_tv");
    entity_row(scr, "icon/area_ctrl.png",         213, 0,        "Persiana cocina",   "cover.persiana_cocina");
    entity_row(scr, "icon/proximity_sensing.png", 213, 0,        "Temperatura salon", "sensor.temp_salon");
}

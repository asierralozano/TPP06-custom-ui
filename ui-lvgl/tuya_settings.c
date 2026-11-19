#include "tuya_settings.h"
#include "fonts.h"
#include "assets.h"
#include "tuya_ui.h"

/* Página de Ajustes estilo Tuya: header + lista de filas (icono + etiqueta +
 * valor/estado + chevron). Iconos reales del firmware (icon/*.png, 36x36).
 * Las opciones son las nuestras: WiFi, Brillo, Fondo, Home Assistant, MQTT,
 * Fecha/hora, Reiniciar — mismo patrón visual que los ajustes de Tuya. */

static int s_row_y;

/* Una fila: tarjeta translúcida 288x58, icono @ (18, centro), etiqueta @ 70,
 * valor gris a la derecha, chevron icon/right.png pegado al borde. */
static void settings_row(lv_obj_t *scr, const char *icon, const char *label,
                         const char *value){
    lv_obj_t *row = lv_obj_create(scr);
    lv_obj_set_size(row, 288, 58);
    lv_obj_set_pos(row, 16, s_row_y);
    s_row_y += 66;
    lv_obj_set_style_radius(row, 16, 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x14141A), 0);
    lv_obj_set_style_bg_opa(row, 230, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Iconos de Tuya con su relleno propio: se renderizan nativos (sin recolor). */
    lv_obj_t *ic = lv_img_create(row);
    lv_img_set_src(ic, asset(icon));
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 20, 0);

    lv_obj_t *lb = lv_label_create(row);
    lv_label_set_text(lb, label);
    lv_obj_set_style_text_font(lb, manrope(17), 0);
    lv_obj_set_style_text_color(lb, lv_color_white(), 0);
    lv_obj_align(lb, LV_ALIGN_LEFT_MID, 70, 0);

    /* Chevron derecha */
    lv_obj_t *ch = lv_img_create(row);
    lv_img_set_src(ch, asset("icon/right.png"));
    lv_obj_set_style_img_recolor(ch, lv_color_hex(0x6A6A72), 0);
    lv_obj_set_style_img_recolor_opa(ch, LV_OPA_COVER, 0);
    lv_obj_align(ch, LV_ALIGN_RIGHT_MID, -18, 0);

    /* Valor/estado gris, a la izquierda del chevron */
    if(value && value[0]){
        lv_obj_t *vl = lv_label_create(row);
        lv_label_set_text(vl, value);
        lv_obj_set_style_text_font(vl, manrope(15), 0);
        lv_obj_set_style_text_color(vl, lv_color_hex(0x8A8A95), 0);
        lv_obj_align(vl, LV_ALIGN_RIGHT_MID, -40, 0);
    }
}

void tuya_settings_build(lv_obj_t *scr){
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0C0C10), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* Header con flecha de volver + título */
    lv_obj_t *back = lv_img_create(scr);
    lv_img_set_src(back, asset("icon/left.png"));
    lv_obj_set_style_img_recolor(back, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(back, LV_OPA_COVER, 0);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 20, 22);
    tuya_ui_attach_back(back);

    lv_obj_t *ttl = lv_label_create(scr);
    lv_label_set_text(ttl, "Ajustes");
    lv_obj_set_style_text_font(ttl, manrope(22), 0);
    lv_obj_set_style_text_color(ttl, lv_color_white(), 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 58, 20);

    /* Lista de opciones (iconos reales del firmware) */
    s_row_y = 78;
    settings_row(scr, "icon/network.png",   "WiFi",              "Conectado");
    settings_row(scr, "icon/brightness.png","Brillo de pantalla","80%");
    settings_row(scr, "icon/display.png",   "Fondo de pantalla", "");
    settings_row(scr, "icon/dev_sys.png",   "Home Assistant",    "OK");
    settings_row(scr, "icon/dev_info.png",  "MQTT",              "mqtt");
    settings_row(scr, "icon/date_time.png", "Reiniciar panel",   "");
}

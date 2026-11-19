#include "tuya_detail.h"
#include "fonts.h"
#include "assets.h"
#include "tuya_ui.h"
#include <stdio.h>

/* Fondo oscuro sólido + header con flecha de volver (icon/left.png) + título.
 * Patrón de category_ui_base.page_create (header 320x70, back @24,21, título 20/600 @58,20). */
void page_bg_header(lv_obj_t *scr, const char *title){
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0C0C10), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *back = lv_img_create(scr);
    lv_img_set_src(back, asset("icon/left.png"));
    lv_obj_set_style_img_recolor(back, lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(back, LV_OPA_COVER, 0);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 20, 20);
    tuya_ui_attach_back(back);

    lv_obj_t *ttl = lv_label_create(scr);
    lv_label_set_text(ttl, title);
    lv_obj_set_style_text_font(ttl, manrope(20), 0);
    lv_obj_set_style_text_color(ttl, lv_color_white(), 0);
    lv_obj_align(ttl, LV_ALIGN_TOP_LEFT, 58, 22);
}

/* --- Detalle de LUZ: barra de brillo vertical grande (como Tuya) + power + % --- */
void tuya_detail_light(lv_obj_t *scr, const char *name, int on, int bright){
    page_bg_header(scr, name);

    /* Barra de brillo vertical (derecha) */
    lv_obj_t *bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 120, 320);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 168, 96);
    lv_obj_set_style_radius(bar, 30, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_radius(bar, 30, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xFFD98A), LV_PART_INDICATOR); /* ámbar cálido */
    lv_bar_set_range(bar, 1, 100);
    lv_bar_set_value(bar, on ? (bright>0?bright:1) : 1, LV_ANIM_OFF);

    /* % grande dentro de la barra (arriba) */
    lv_obj_t *pct = lv_label_create(scr);
    char buf[16]; snprintf(buf, sizeof buf, "%d%%", on ? (bright>0?bright:0) : 0);
    lv_label_set_text(pct, buf);
    lv_obj_set_style_text_font(pct, manrope(26), 0);
    lv_obj_set_style_text_color(pct, lv_color_hex(on?0x3A2E12:0x6A6A72), 0);
    lv_obj_align(pct, LV_ALIGN_TOP_LEFT, 168+28, 96+18);

    /* Columna izquierda: botón power + etiqueta */
    lv_obj_t *pw = lv_img_create(scr);
    lv_img_set_src(pw, asset(on ? "icon/on.png" : "icon/off.png"));
    lv_obj_align(pw, LV_ALIGN_TOP_LEFT, 44, 150);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, on ? "Encendida" : "Apagada");
    lv_obj_set_style_text_font(lbl, manrope(18), 0);
    lv_obj_set_style_text_color(lbl, on ? lv_color_hex(0x9AE6C0) : lv_color_hex(0x8A8A95), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 40, 220);

    lv_obj_t *cap = lv_label_create(scr);
    lv_label_set_text(cap, "Brillo");
    lv_obj_set_style_text_font(cap, manrope(15), 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0x6A6A72), 0);
    lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 40, 258);
}

/* --- Detalle de COVER/persiana: abrir/parar/cerrar + posición --- */
static lv_obj_t *cover_btn(lv_obj_t *scr, int y, const char *txt, uint32_t col){
    lv_obj_t *b = lv_obj_create(scr);
    lv_obj_set_size(b, 240, 68);
    lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(b, 18, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1C1C1E), 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, manrope(20), 0);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    lv_obj_center(l);
    return b;
}

/* --- Detalle de TERMOSTATO (de thermoregulator.lua): setpoint grande + °C
 * superíndice + botones ± a los lados + temp actual + modo. --- */
static lv_obj_t *round_btn(lv_obj_t *scr, int dx, const char *glyph){
    lv_obj_t *b = lv_obj_create(scr);
    lv_obj_set_size(b, 72, 72);
    lv_obj_align(b, LV_ALIGN_TOP_MID, dx, 300);
    lv_obj_set_style_radius(b, 36, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x1C1C22), 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, glyph);
    lv_obj_set_style_text_font(l, manrope(40), 0);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_center(l);
    return b;
}

void tuya_detail_thermostat(lv_obj_t *scr, const char *name, int setpoint,
                            int current, const char *mode){
    page_bg_header(scr, name);

    /* Setpoint grande centrado, Manrope 67/700, y112 */
    lv_obj_t *sp = lv_label_create(scr);
    char buf[16]; snprintf(buf, sizeof buf, "%d", setpoint);
    lv_label_set_text(sp, buf);
    lv_obj_set_style_text_font(sp, manrope(67), 0);
    lv_obj_set_style_text_color(sp, lv_color_white(), 0);
    lv_obj_align(sp, LV_ALIGN_TOP_MID, -14, 112);

    /* Unidad °C como superíndice, abajo-derecha del valor */
    lv_obj_t *unit = lv_label_create(scr);
    lv_label_set_text(unit, "°C");
    lv_obj_set_style_text_font(unit, manrope(20), 0);
    lv_obj_set_style_text_color(unit, lv_color_hex(0xB8B8C0), 0);
    lv_obj_align_to(unit, sp, LV_ALIGN_OUT_RIGHT_TOP, 6, 14);

    /* Modo (verde), y206 */
    lv_obj_t *md = lv_label_create(scr);
    lv_label_set_text(md, mode);
    lv_obj_set_style_text_font(md, manrope(20), 0);
    lv_obj_set_style_text_color(md, lv_color_hex(0x57C891), 0);
    lv_obj_set_width(md, 320);
    lv_obj_set_style_text_align(md, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(md, LV_ALIGN_TOP_MID, 0, 206);

    /* Botones - / + a los lados */
    round_btn(scr, -93, "-");
    round_btn(scr,  93, "+");

    /* Temperatura actual, abajo */
    lv_obj_t *cur = lv_label_create(scr);
    char cbuf[32]; snprintf(cbuf, sizeof cbuf, "Actual: %d °C", current);
    lv_label_set_text(cur, cbuf);
    lv_obj_set_style_text_font(cur, manrope(15), 0);
    lv_obj_set_style_text_color(cur, lv_color_hex(0x8A8A95), 0);
    lv_obj_set_width(cur, 320);
    lv_obj_set_style_text_align(cur, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(cur, LV_ALIGN_TOP_MID, 0, 404);
}

void tuya_detail_cover(lv_obj_t *scr, const char *name, int position){
    page_bg_header(scr, name);

    /* Posición actual, grande y centrada */
    lv_obj_t *pos = lv_label_create(scr);
    char buf[24]; snprintf(buf, sizeof buf, "%d%%", position);
    lv_label_set_text(pos, buf);
    lv_obj_set_style_text_font(pos, manrope(48), 0);
    lv_obj_set_style_text_color(pos, lv_color_white(), 0);
    lv_obj_align(pos, LV_ALIGN_TOP_MID, 0, 86);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub, "Abierta");
    lv_obj_set_style_text_font(sub, manrope(15), 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(0x8A8A95), 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 150);

    /* Botones abrir / parar / cerrar */
    cover_btn(scr, 190, "Abrir",  0x6F7BFF);
    cover_btn(scr, 268, "Parar",  0xF0B24F);
    cover_btn(scr, 346, "Cerrar", 0x57C891);
}

#include "tuya_media.h"
#include "tuya_detail.h"
#include "fonts.h"
#include "assets.h"
#include <stdio.h>

/* Reproductor (media_player de HA). El firmware Tuya no trae player de música
 * nativo (su media/ es el intercomunicador), así que esta pantalla es un diseño
 * ORIGINAL en el lenguaje visual Tuya: carátula con degradado propio (sin arte
 * con copyright) + iconos de transporte reales del firmware (btn_left/pause/right). */

/* Marca de nota musical dibujada con primitivas (original): cabeza + mástil. */
static void note_mark(lv_obj_t *parent){
    lv_obj_t *head = lv_obj_create(parent);
    lv_obj_set_size(head, 26, 20);
    lv_obj_align(head, LV_ALIGN_CENTER, -6, 14);
    lv_obj_set_style_radius(head, 10, 0);
    lv_obj_set_style_bg_color(head, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(head, 220, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_clear_flag(head, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *stem = lv_obj_create(parent);
    lv_obj_set_size(stem, 4, 44);
    lv_obj_align(stem, LV_ALIGN_CENTER, 8, -4);
    lv_obj_set_style_radius(stem, 2, 0);
    lv_obj_set_style_bg_color(stem, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(stem, 220, 0);
    lv_obj_set_style_border_width(stem, 0, 0);
    lv_obj_clear_flag(stem, LV_OBJ_FLAG_SCROLLABLE);
}

/* Carátula original: caja redondeada con degradado azul→morado + nota. */
static lv_obj_t *album_art(lv_obj_t *parent, int size){
    lv_obj_t *art = lv_obj_create(parent);
    lv_obj_set_size(art, size, size);
    lv_obj_set_style_radius(art, size/6, 0);
    lv_obj_set_style_bg_color(art, lv_color_hex(0x6F7BFF), 0);
    lv_obj_set_style_bg_grad_color(art, lv_color_hex(0xB06FE0), 0);
    lv_obj_set_style_bg_grad_dir(art, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(art, 0, 0);
    lv_obj_set_style_pad_all(art, 0, 0);
    lv_obj_clear_flag(art, LV_OBJ_FLAG_SCROLLABLE);
    if(size >= 120) note_mark(art);
    return art;
}

/* Botón redondo de transporte con un icono del firmware. accent!=0 => relleno. */
static void transport_btn(lv_obj_t *scr, int dx, int dy, int d, const char *icon,
                          uint32_t accent){
    lv_obj_t *b = lv_obj_create(scr);
    lv_obj_set_size(b, d, d);
    lv_obj_align(b, LV_ALIGN_TOP_MID, dx, dy);
    lv_obj_set_style_radius(b, d/2, 0);
    lv_obj_set_style_bg_color(b, accent ? lv_color_hex(accent) : lv_color_hex(0x1C1C22), 0);
    lv_obj_set_style_bg_opa(b, accent ? LV_OPA_COVER : 0, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *ic = lv_img_create(b);
    lv_img_set_src(ic, asset(icon));
    lv_obj_set_style_img_recolor(ic, accent ? lv_color_hex(0x06281C) : lv_color_white(), 0);
    lv_obj_set_style_img_recolor_opa(ic, LV_OPA_COVER, 0);
    lv_obj_center(ic);
}

/* --- Tarjeta de media (home): carátula mini + título/artista + play/pause. --- */
lv_obj_t *tuya_card_media(lv_obj_t *parent, int x, int y, int w,
                          const char *title, const char *artist, int playing){
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, 96);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x14141A), 0);
    lv_obj_set_style_bg_opa(card, 185, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *art = album_art(card, 56);
    lv_obj_align(art, LV_ALIGN_LEFT_MID, 14, 0);

    lv_obj_t *tt = lv_label_create(card);
    lv_label_set_text(tt, title);
    lv_obj_set_style_text_font(tt, manrope(17), 0);
    lv_obj_set_style_text_color(tt, lv_color_white(), 0);
    lv_obj_align(tt, LV_ALIGN_LEFT_MID, 84, -12);

    lv_obj_t *ar = lv_label_create(card);
    lv_label_set_text(ar, artist);
    lv_obj_set_style_text_font(ar, manrope(14), 0);
    lv_obj_set_style_text_color(ar, lv_color_hex(0x8A8A95), 0);
    lv_obj_align(ar, LV_ALIGN_LEFT_MID, 84, 12);

    transport_btn(card, 0, 0, 52, "icon/btn_pause.png", playing ? 0x57C891 : 0);
    lv_obj_t *pb = lv_obj_get_child(card, lv_obj_get_child_cnt(card)-1);
    lv_obj_align(pb, LV_ALIGN_RIGHT_MID, -14, 0);
    return card;
}

/* --- Detalle de media a pantalla completa --- */
void tuya_detail_media(lv_obj_t *scr, const char *name, const char *title,
                       const char *artist, int playing, int volume){
    page_bg_header(scr, name);

    /* Carátula grande (original) */
    lv_obj_t *art = album_art(scr, 176);
    lv_obj_align(art, LV_ALIGN_TOP_MID, 0, 84);

    /* Título + artista */
    lv_obj_t *tt = lv_label_create(scr);
    lv_label_set_text(tt, title);
    lv_obj_set_style_text_font(tt, manrope(22), 0);
    lv_obj_set_style_text_color(tt, lv_color_white(), 0);
    lv_obj_set_width(tt, 300);
    lv_obj_set_style_text_align(tt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(tt, LV_ALIGN_TOP_MID, 0, 280);

    lv_obj_t *ar = lv_label_create(scr);
    lv_label_set_text(ar, artist);
    lv_obj_set_style_text_font(ar, manrope(16), 0);
    lv_obj_set_style_text_color(ar, lv_color_hex(0x8A8A95), 0);
    lv_obj_set_width(ar, 300);
    lv_obj_set_style_text_align(ar, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(ar, LV_ALIGN_TOP_MID, 0, 312);

    /* Barra de progreso */
    lv_obj_t *prog = lv_bar_create(scr);
    lv_obj_set_size(prog, 272, 6);
    lv_obj_align(prog, LV_ALIGN_TOP_MID, 0, 348);
    lv_obj_set_style_radius(prog, 3, 0);
    lv_obj_set_style_bg_color(prog, lv_color_hex(0x2A2A32), 0);
    lv_obj_set_style_bg_color(prog, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(prog, 3, LV_PART_INDICATOR);
    lv_bar_set_value(prog, 42, LV_ANIM_OFF);

    /* Transporte: prev · play/pause · next */
    transport_btn(scr, -96, 374, 56, "icon/btn_left.png",  0);
    transport_btn(scr,   0, 366, 76, "icon/btn_pause.png", playing ? 0x57C891 : 0xFFFFFF);
    transport_btn(scr,  96, 374, 56, "icon/btn_right.png", 0);

    /* Volumen: icono + slider */
    lv_obj_t *vic = lv_img_create(scr);
    lv_img_set_src(vic, asset("icon/5inch_volume.png"));
    lv_obj_set_style_img_recolor(vic, lv_color_hex(0x8A8A95), 0);
    lv_obj_set_style_img_recolor_opa(vic, LV_OPA_COVER, 0);
    lv_obj_align(vic, LV_ALIGN_TOP_LEFT, 24, 452);

    lv_obj_t *vol = lv_bar_create(scr);
    lv_obj_set_size(vol, 232, 6);
    lv_obj_align(vol, LV_ALIGN_TOP_LEFT, 64, 463);
    lv_obj_set_style_radius(vol, 3, 0);
    lv_obj_set_style_bg_color(vol, lv_color_hex(0x2A2A32), 0);
    lv_obj_set_style_bg_color(vol, lv_color_hex(0x57C891), LV_PART_INDICATOR);
    lv_obj_set_style_radius(vol, 3, LV_PART_INDICATOR);
    lv_bar_set_value(vol, volume, LV_ANIM_OFF);
}

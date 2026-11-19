#ifndef TUYA_MEDIA_H
#define TUYA_MEDIA_H
#include "lvgl/lvgl.h"
/* Reproductor (media_player de HA): diseño propio en lenguaje visual Tuya. */
lv_obj_t *tuya_card_media(lv_obj_t *parent, int x, int y, int w,
                          const char *title, const char *artist, int playing);
void tuya_detail_media(lv_obj_t *scr, const char *name, const char *title,
                       const char *artist, int playing, int volume);
#endif

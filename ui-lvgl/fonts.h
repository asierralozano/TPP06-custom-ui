#ifndef FONTS_H
#define FONTS_H
#include "lvgl/lvgl.h"
/* Manrope en runtime (tiny_ttf), cualquier tamaño; cacheado por tamaño.
   Equivalente a gui_custom_vfont_get de la UI de Tuya. */
lv_font_t *manrope(int size);
#endif

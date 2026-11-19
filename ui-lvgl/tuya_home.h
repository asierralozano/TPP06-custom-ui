#ifndef TUYA_HOME_H
#define TUYA_HOME_H
#include "lvgl/lvgl.h"
/* Construye el home estilo Tuya (pantalla 0: reloj/escritorio) en `scr`.
   Recreación 1:1 de theme/desktop.lua (estilo 0). Textos de muestra por ahora. */
void tuya_home_build(lv_obj_t *scr);
#endif

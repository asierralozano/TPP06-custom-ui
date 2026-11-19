#ifndef TUYA_UI_H
#define TUYA_UI_H
#include "lvgl/lvgl.h"
/* Callback de "volver" compartido por todas las cabeceras (lo fija el router). */
void tuya_ui_set_back_cb(void (*cb)(void));
/* Engancha una imagen de flecha de volver al callback (la hace clicable). */
void tuya_ui_attach_back(lv_obj_t *back_img);
#endif

#ifndef TUYA_DETAIL_H
#define TUYA_DETAIL_H
#include "lvgl/lvgl.h"
/* Páginas de detalle a pantalla completa, estilo Tuya (header + controles por tipo). */
/* Header común: fondo oscuro + flecha de volver + título (compartido por detalles). */
void page_bg_header(lv_obj_t *scr, const char *title);
void tuya_detail_light(lv_obj_t *scr, const char *name, int on, int bright);
void tuya_detail_cover(lv_obj_t *scr, const char *name, int position);
void tuya_detail_thermostat(lv_obj_t *scr, const char *name, int setpoint,
                            int current, const char *mode);
#endif

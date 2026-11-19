#ifndef TUYA_CARDS_H
#define TUYA_CARDS_H
#include "lvgl/lvgl.h"
/* Tarjeta de dispositivo estilo Tuya (home). bright: 0-100, o -1 si no aplica. */
lv_obj_t *tuya_card(lv_obj_t *parent, int x, int y, int w,
                    const char *name, const char *sub, int on, int bright);
/* Tarjeta de sensor (temp/humedad lado a lado). */
lv_obj_t *tuya_card_sensor(lv_obj_t *parent, int x, int y, int w,
                           const char *name, const char *temp, const char *humi);
/* Página de dispositivos de demo (wallpaper + título + tarjetas). */
void tuya_devices_build(lv_obj_t *scr);
/* Página "catálogo": muestra sensor + luz + interruptor juntos. */
void tuya_devices_mix_build(lv_obj_t *scr);
#endif

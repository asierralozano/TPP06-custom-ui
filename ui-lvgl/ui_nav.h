#ifndef UI_NAV_H
#define UI_NAV_H
#include "lvgl/lvgl.h"

/* Router de pantallas de la UI Tuya. Gestiona una pila para "volver" y construye
 * cada pantalla bajo demanda llamando al módulo tuya_* correspondiente.
 *
 * ANDAMIAJE: hoy los detalles se dibujan con el contexto de abajo (datos demo).
 * Para enchufar a HA: antes de ui_nav_push(SCR_DETAIL_*), rellenar g_nav_ctx
 * desde la ha_entity seleccionada (name/state/bright/position/...), y en los
 * callbacks de control llamar a ha_call()/ha_call_bri()/... (ver NOTAS-INTEGRACION).
 */
typedef enum {
    SCR_HOME = 0,        /* reloj / escritorio (tuya_home) */
    SCR_DEVICES,         /* tarjetas de dispositivos (tuya_devices) */
    SCR_SCENES,          /* escenas (tuya_scenes) */
    SCR_CONTROL,         /* centro de control (tuya_control_center) */
    SCR_SETTINGS,        /* ajustes (tuya_settings) */
    SCR_ADD_ENTITY,      /* wizard añadir entidad (tuya_add_entity) */
    SCR_SCREENSAVER,     /* salvapantallas (tuya_screensaver) */
    SCR_DETAIL_LIGHT,    /* detalle luz */
    SCR_DETAIL_COVER,    /* detalle cover */
    SCR_DETAIL_THERMO,   /* detalle termostato */
    SCR_DETAIL_MEDIA,    /* detalle media_player */
    SCR_COUNT
} ui_screen_t;

/* Contexto para la pantalla de detalle a construir (rellenar antes de push). */
typedef struct {
    char name[40];
    char line1[40];      /* estado / título */
    char line2[40];      /* subestado / artista / modo */
    int  on;
    int  bright;         /* 0-100 */
    int  position;       /* cover 0-100 */
    int  setpoint;       /* termostato */
    int  current;        /* termostato temp actual */
    int  volume;         /* media 0-100 */
    int  playing;        /* media */
} ui_nav_ctx_t;

extern ui_nav_ctx_t g_nav_ctx;

void ui_nav_init(void);              /* crea el router y muestra SCR_HOME */
void ui_nav_go(ui_screen_t s);       /* navegación lateral: reinicia la pila a [s] */
void ui_nav_push(ui_screen_t s);     /* apila s sobre la actual (back vuelve) */
void ui_nav_back(void);              /* desapila; si la pila queda vacía, HOME */
ui_screen_t ui_nav_current(void);

#endif

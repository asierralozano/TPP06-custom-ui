#include "ui_nav.h"
#include "tuya_ui.h"
#include "tuya_home.h"
#include "tuya_cards.h"
#include "tuya_detail.h"
#include "tuya_scenes.h"
#include "tuya_settings.h"
#include "tuya_control_center.h"
#include "tuya_add_entity.h"
#include "tuya_screensaver.h"
#include "tuya_media.h"

/* Router de pantallas. Mantiene una pila de ui_screen_t y, en cada navegación,
 * construye una pantalla LVGL nueva y la carga con animación (auto_del borra la
 * anterior). Los detalles leen g_nav_ctx. Ver ui_nav.h para la integración HA. */

ui_nav_ctx_t g_nav_ctx;

#define NAV_STACK_MAX 8
static ui_screen_t s_stack[NAV_STACK_MAX];
static int s_sp = 0;                 /* tope de pila (nº de elementos) */

/* Construye la pantalla s en un screen LVGL nuevo y lo devuelve. */
static lv_obj_t *build_screen(ui_screen_t s){
    lv_obj_t *scr = lv_obj_create(NULL);
    switch(s){
        case SCR_HOME:        tuya_home_build(scr); break;
        case SCR_DEVICES:     tuya_devices_build(scr); break;
        case SCR_SCENES:      tuya_scenes_build(scr); break;
        case SCR_CONTROL:     tuya_control_center_build(scr); break;
        case SCR_SETTINGS:    tuya_settings_build(scr); break;
        case SCR_ADD_ENTITY:  tuya_add_entity_build(scr); break;
        case SCR_SCREENSAVER: tuya_screensaver_build(scr); break;
        case SCR_DETAIL_LIGHT:
            tuya_detail_light(scr, g_nav_ctx.name, g_nav_ctx.on, g_nav_ctx.bright); break;
        case SCR_DETAIL_COVER:
            tuya_detail_cover(scr, g_nav_ctx.name, g_nav_ctx.position); break;
        case SCR_DETAIL_THERMO:
            tuya_detail_thermostat(scr, g_nav_ctx.name, g_nav_ctx.setpoint,
                                   g_nav_ctx.current, g_nav_ctx.line2); break;
        case SCR_DETAIL_MEDIA:
            tuya_detail_media(scr, g_nav_ctx.name, g_nav_ctx.line1,
                              g_nav_ctx.line2, g_nav_ctx.playing, g_nav_ctx.volume); break;
        default:              tuya_home_build(scr); break;
    }
    return scr;
}

/* Elige la transición según si entramos (push) o volvemos (back/lateral). */
static void load(ui_screen_t s, lv_scr_load_anim_t anim){
    lv_obj_t *scr = build_screen(s);
    /* auto_del=true borra la pantalla saliente al terminar la animación. */
    lv_scr_load_anim(scr, anim, 180, 0, true);
}

void ui_nav_init(void){
    tuya_ui_set_back_cb(ui_nav_back);
    s_sp = 0;
    s_stack[s_sp++] = SCR_HOME;
    load(SCR_HOME, LV_SCR_LOAD_ANIM_NONE);
}

void ui_nav_go(ui_screen_t s){
    /* Navegación lateral (barra/gestos): la pila se reinicia a [s]. */
    s_sp = 0;
    s_stack[s_sp++] = s;
    load(s, LV_SCR_LOAD_ANIM_FADE_ON);
}

void ui_nav_push(ui_screen_t s){
    if(s_sp >= NAV_STACK_MAX) s_sp = NAV_STACK_MAX - 1;
    s_stack[s_sp++] = s;
    load(s, LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

void ui_nav_back(void){
    if(s_sp <= 1){                   /* ya en la raíz: a HOME */
        ui_nav_go(SCR_HOME);
        return;
    }
    s_sp--;                          /* quita la actual */
    load(s_stack[s_sp - 1], LV_SCR_LOAD_ANIM_MOVE_RIGHT);
}

ui_screen_t ui_nav_current(void){
    return s_sp > 0 ? s_stack[s_sp - 1] : SCR_HOME;
}

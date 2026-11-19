/* Render headless de la UI en el Mac (sin panel): LVGL -> buffer RGB -> BMP.
 * Permite ver/ajustar la UI al píxel antes de tener el panel por UART. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "lvgl/lvgl.h"
#include "tuya_home.h"
#include "tuya_cards.h"
#include "tuya_detail.h"
#include "tuya_screensaver.h"
#include "tuya_settings.h"
#include "tuya_control_center.h"
#include "tuya_add_entity.h"
#include "tuya_scenes.h"
#include "tuya_media.h"

#define W 320
#define H 480
static lv_color_t dbuf[W*H];
static uint8_t fbrgb[W*H*3];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *cp){
    for(int y=area->y1; y<=area->y2; y++){
        for(int x=area->x1; x<=area->x2; x++){
            lv_color_t c = *cp++;
            uint16_t v = c.full;                 /* RGB565 (LV_COLOR_16_SWAP 0) */
            uint8_t r=(v>>11)&0x1F, g=(v>>5)&0x3F, b=v&0x1F;
            int i=(y*W+x)*3;
            fbrgb[i]   = (r<<3)|(r>>2);
            fbrgb[i+1] = (g<<2)|(g>>4);
            fbrgb[i+2] = (b<<3)|(b>>2);
        }
    }
    lv_disp_flush_ready(drv);
}

static void write_bmp(const char *path){
    int rowsize=(W*3+3)&~3, imgsize=rowsize*H, filesize=54+imgsize;
    unsigned char h[54]={0};
    h[0]='B';h[1]='M';
    h[2]=filesize;h[3]=filesize>>8;h[4]=filesize>>16;h[5]=filesize>>24;
    h[10]=54;h[14]=40;
    h[18]=W&0xFF;h[19]=(W>>8)&0xFF;
    h[22]=H&0xFF;h[23]=(H>>8)&0xFF;
    h[26]=1;h[28]=24;
    FILE*f=fopen(path,"wb"); if(!f){perror("fopen");return;}
    fwrite(h,1,54,f);
    unsigned char pad[3]={0,0,0};
    for(int y=H-1;y>=0;y--){
        for(int x=0;x<W;x++){
            int i=(y*W+x)*3;
            unsigned char bgr[3]={fbrgb[i+2],fbrgb[i+1],fbrgb[i]};
            fwrite(bgr,1,3,f);
        }
        fwrite(pad,1,rowsize-W*3,f);
    }
    fclose(f);
}

int main(void){
    lv_init();
    static lv_disp_draw_buf_t ddb;
    lv_disp_draw_buf_init(&ddb, dbuf, NULL, W*H);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.draw_buf=&ddb; drv.flush_cb=flush_cb; drv.hor_res=W; drv.ver_res=H;
    lv_disp_drv_register(&drv);

    /* Pantalla 1: home (reloj) */
    tuya_home_build(lv_scr_act());
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/home.bmp");
    printf("wrote sim/home.bmp\n");

    /* Pantalla 2: dispositivos (tarjetas) */
    lv_obj_t *scr2 = lv_obj_create(NULL);
    lv_scr_load(scr2);
    tuya_devices_build(scr2);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/devices.bmp");
    printf("wrote sim/devices.bmp\n");

    /* Pantalla 3: detalle de luz */
    lv_obj_t *scr3 = lv_obj_create(NULL);
    lv_scr_load(scr3);
    tuya_detail_light(scr3, "Mesa", 1, 80);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/light_detail.bmp");
    printf("wrote sim/light_detail.bmp\n");

    /* Pantalla 4: detalle de cover */
    lv_obj_t *scr4 = lv_obj_create(NULL);
    lv_scr_load(scr4);
    tuya_detail_cover(scr4, "Persiana", 65);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/cover_detail.bmp");
    printf("wrote sim/cover_detail.bmp\n");

    /* Pantalla 5: salvapantallas (HH/MM apiladas) */
    lv_obj_t *scr5 = lv_obj_create(NULL);
    lv_scr_load(scr5);
    tuya_screensaver_build(scr5);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/screensaver.bmp");
    printf("wrote sim/screensaver.bmp\n");

    /* Pantalla 6: ajustes */
    lv_obj_t *scr6 = lv_obj_create(NULL);
    lv_scr_load(scr6);
    tuya_settings_build(scr6);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/settings.bmp");
    printf("wrote sim/settings.bmp\n");

    /* Pantalla 7: centro de control */
    lv_obj_t *scr7 = lv_obj_create(NULL);
    lv_scr_load(scr7);
    tuya_control_center_build(scr7);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/control_center.bmp");
    printf("wrote sim/control_center.bmp\n");

    /* Pantalla 8: wizard añadir entidad */
    lv_obj_t *scr8 = lv_obj_create(NULL);
    lv_scr_load(scr8);
    tuya_add_entity_build(scr8);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/add_entity.bmp");
    printf("wrote sim/add_entity.bmp\n");

    /* Pantalla 9: detalle de termostato */
    lv_obj_t *scr9 = lv_obj_create(NULL);
    lv_scr_load(scr9);
    tuya_detail_thermostat(scr9, "Termostato", 21, 19, "Calefaccion");
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/thermostat_detail.bmp");
    printf("wrote sim/thermostat_detail.bmp\n");

    /* Pantalla 10: catálogo de tipos de tarjeta (sensor+luz+interruptor) */
    lv_obj_t *scr10 = lv_obj_create(NULL);
    lv_scr_load(scr10);
    tuya_devices_mix_build(scr10);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/devices_mix.bmp");
    printf("wrote sim/devices_mix.bmp\n");

    /* Pantalla 11: escenas */
    lv_obj_t *scr11 = lv_obj_create(NULL);
    lv_scr_load(scr11);
    tuya_scenes_build(scr11);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/scenes.bmp");
    printf("wrote sim/scenes.bmp\n");

    /* Pantalla 12: detalle de media player */
    lv_obj_t *scr12 = lv_obj_create(NULL);
    lv_scr_load(scr12);
    tuya_detail_media(scr12, "Salon", "Pista demo", "Artista", 1, 55);
    for(int i=0;i<30;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp("sim/media_detail.bmp");
    printf("wrote sim/media_detail.bmp\n");
    return 0;
}

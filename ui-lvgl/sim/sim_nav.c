/* Driver de simulación del router (ui_nav): ejercita navegación real
 * (HOME -> go DEVICES -> push DETALLE luz -> back) y renderiza cada paso a BMP.
 * Verifica el andamiaje de navegación sin el panel. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "ui_nav.h"

#define W 320
#define H 480
static lv_color_t dbuf[W*H];
static uint8_t fbrgb[W*H*3];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *cp){
    for(int y=area->y1; y<=area->y2; y++)
        for(int x=area->x1; x<=area->x2; x++){
            uint16_t v = (*cp++).full;
            uint8_t r=(v>>11)&0x1F, g=(v>>5)&0x3F, b=v&0x1F;
            int i=(y*W+x)*3;
            fbrgb[i]=(r<<3)|(r>>2); fbrgb[i+1]=(g<<2)|(g>>4); fbrgb[i+2]=(b<<3)|(b>>2);
        }
    lv_disp_flush_ready(drv);
}

static void write_bmp(const char *path){
    int rowsize=(W*3+3)&~3, imgsize=rowsize*H, filesize=54+imgsize;
    unsigned char h[54]={0};
    h[0]='B';h[1]='M'; h[2]=filesize;h[3]=filesize>>8;h[4]=filesize>>16;h[5]=filesize>>24;
    h[10]=54;h[14]=40; h[18]=W&0xFF;h[19]=(W>>8)&0xFF; h[22]=H&0xFF;h[23]=(H>>8)&0xFF;
    h[26]=1;h[28]=24;
    FILE*f=fopen(path,"wb"); if(!f){perror("fopen");return;}
    fwrite(h,1,54,f);
    unsigned char pad[3]={0,0,0};
    for(int y=H-1;y>=0;y--){
        for(int x=0;x<W;x++){ int i=(y*W+x)*3; unsigned char bgr[3]={fbrgb[i+2],fbrgb[i+1],fbrgb[i]}; fwrite(bgr,1,3,f); }
        fwrite(pad,1,rowsize-W*3,f);
    }
    fclose(f);
}

/* deja correr LVGL para que termine la animación de carga y refresque */
static void settle(const char *path){
    for(int i=0;i<45;i++){ lv_timer_handler(); lv_tick_inc(16); }
    write_bmp(path);
    printf("wrote %s (pantalla actual=%d)\n", path, (int)ui_nav_current());
}

int main(void){
    lv_init();
    static lv_disp_draw_buf_t ddb;
    lv_disp_draw_buf_init(&ddb, dbuf, NULL, W*H);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.draw_buf=&ddb; drv.flush_cb=flush_cb; drv.hor_res=W; drv.ver_res=H;
    lv_disp_drv_register(&drv);

    /* 1) Arranque: HOME */
    ui_nav_init();
    settle("sim/nav_1_home.bmp");

    /* 2) Navegación lateral a DISPOSITIVOS */
    ui_nav_go(SCR_DEVICES);
    settle("sim/nav_2_devices.bmp");

    /* 3) Abrir un detalle de luz (rellenando el contexto como hará HA) */
    strcpy(g_nav_ctx.name, "Mesa");
    g_nav_ctx.on = 1; g_nav_ctx.bright = 72;
    ui_nav_push(SCR_DETAIL_LIGHT);
    settle("sim/nav_3_detail.bmp");

    /* 4) Volver (debe regresar a DISPOSITIVOS) */
    ui_nav_back();
    settle("sim/nav_4_back.bmp");

    /* 5) Centro de control */
    ui_nav_go(SCR_CONTROL);
    settle("sim/nav_5_control.bmp");

    return 0;
}

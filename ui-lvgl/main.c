/* tpp06_lvgl — UI del TPP06 con LVGL (Fase A: inicio funcional).
 *
 * Panel 320x480, 16bpp, stride 640. LCD BGR565 (LVGL=RGB565 -> swap R/B en flush).
 * Present por write() a /dev/fb0 (Tuya MATADA). Táctil goodix event1.
 * Config /tuya/data/custom/tpp06_ui.conf: ha_host/ha_port/ha_token,
 *   relay=N|Etiqueta  (relé KC_CTLn local)
 *   widget=entity_id|Etiqueta|dominio  (entidad HA)
 * Botones físicos key_f1/2/3 (gpio 62/63/64) por debugfs -> conmutan el relé N.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/fb.h>
#include <linux/input.h>
#include "lvgl/lvgl.h"
#include "ha_ws.h"
#include "fonts.h"

#ifndef FBIOBLANK
#define FBIOBLANK 0x4611
#endif

/* ------------------------------ framebuffer ------------------------------ */
static int fbfd=-1, tpfd=-1;
static int SW=320, SH=480, STRIDE=640, FBLEN=307200;
static size_t SMEM=614400;
static uint16_t *fbout=NULL;
static volatile int g_run=1;
static void on_sig(int s){ (void)s; g_run=0; }

static int fb_init(void){
    fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd < 0){ perror("fb0"); return -1; }
    struct fb_var_screeninfo v; struct fb_fix_screeninfo f;
    ioctl(fbfd, FBIOGET_VSCREENINFO, &v);
    ioctl(fbfd, FBIOGET_FSCREENINFO, &f);
    SW=v.xres; SH=v.yres; STRIDE=f.line_length?f.line_length:SW*2; FBLEN=STRIDE*SH;
    SMEM=f.smem_len?f.smem_len:(size_t)FBLEN;
    fbout = malloc(FBLEN);
    if(!fbout) return -1;
    ioctl(fbfd, FBIOBLANK, 0);
    int b=open("/sys/class/backlight/backlight/bl_power",O_WRONLY); if(b>=0){ if(write(b,"0\n",2)<0){} close(b);}
    b=open("/sys/class/backlight/backlight/brightness",O_WRONLY); if(b>=0){ if(write(b,"200\n",4)<0){} close(b);}
    return 0;
}
static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *cp){
    for (int y=area->y1; y<=area->y2; y++){
        uint16_t *row = fbout + y*SW;
        for (int x=area->x1; x<=area->x2; x++){
            uint16_t c = cp->full;
            uint16_t r=(c>>11)&0x1f, g=(c>>5)&0x3f, b=c&0x1f;
            row[x] = (uint16_t)((b<<11)|(g<<5)|r);
            cp++;
        }
    }
    if (lv_disp_flush_is_last(drv)){
        int nbuf = (SMEM >= 2*(size_t)FBLEN) ? 2 : 1;
        for (int bi=0; bi<nbuf; bi++){
            if (lseek(fbfd,(off_t)bi*FBLEN,SEEK_SET)==(off_t)-1) break;
            size_t off=0;
            while (off<(size_t)FBLEN){
                ssize_t w=write(fbfd,(const uint8_t*)fbout+off,FBLEN-off);
                if (w<=0){ if(errno==EINTR) continue; break; }
                off+=(size_t)w;
            }
        }
    }
    lv_disp_flush_ready(drv);
}

/* -------------------------------- táctil --------------------------------- */
static int tpx=0, tpy=0, tpdown=0;
static void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data){
    (void)drv; struct input_event ev;
    while (read(tpfd,&ev,sizeof ev)==(int)sizeof ev){
        if (ev.type==EV_ABS){
            if (ev.code==ABS_MT_POSITION_X||ev.code==ABS_X) tpx=ev.value;
            else if (ev.code==ABS_MT_POSITION_Y||ev.code==ABS_Y) tpy=ev.value;
            else if (ev.code==ABS_MT_TRACKING_ID) tpdown=(ev.value!=-1);
        } else if (ev.type==EV_KEY && ev.code==BTN_TOUCH){ tpdown=ev.value?1:0; }
    }
    data->point.x=tpx; data->point.y=tpy;
    data->state = tpdown ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* --------------------------- relés y botones ----------------------------- */
static void relay_set(int n, int on){
    char p[80];
    snprintf(p,sizeof p,"/sys/class/gpio/KC_CTL%d/value",n);
    int fd=open(p,O_WRONLY); if(fd>=0){ if(write(fd,on?"1\n":"0\n",2)<0){} close(fd); }
    snprintf(p,sizeof p,"/sys/class/leds/button%d_led/brightness",n);
    fd=open(p,O_WRONLY); if(fd>=0){ if(write(fd,on?"255\n":"0\n",on?4:2)<0){} close(fd); }
}
static int relay_get(int n){
    char p[80]; snprintf(p,sizeof p,"/sys/class/gpio/KC_CTL%d/value",n);
    int fd=open(p,O_RDONLY); if(fd<0) return -1;
    char b=0; int r=read(fd,&b,1); close(fd); if(r<=0) return -1;
    return (b=='1');
}
static int read_buttons(void){
    int fd=open("/sys/kernel/debug/gpio",O_RDONLY); if(fd<0) return 0;
    char buf[8192]; int n=read(fd,buf,sizeof buf-1); close(fd);
    if(n<=0) return 0; buf[n]=0;
    int m=0;
    for(int k=0;k<3;k++){
        char name[8]; snprintf(name,sizeof name,"key_f%d",k+1);
        char*p=strstr(buf,name); if(!p) continue;
        char*eol=strchr(p,'\n'); if(!eol) eol=buf+n;
        char c=*eol; *eol=0; if(strstr(p," lo")) m|=(1<<k); *eol=c;
    }
    return m;
}

/* -------------------------------- items ---------------------------------- */
typedef struct {
    int kind;                 /* 0=HA, 1=relé */
    int relay;
    int page;                 /* página a la que pertenece */
    char entity[HA_ID_LEN];
    char domain[16];
    char label[32];
    int state;                /* 0/1/-1 */
    lv_obj_t *tile, *icon, *name_lbl;
} Item;
static Item items[16]; static int n_items=0;
static int g_edit=0;                 /* modo editar activo */
static int g_cur_page=0;             /* página visible actual */
static int g_edit_page=0;            /* página que se está editando */
static int g_wall=0;                 /* índice de fondo de pantalla */
static int g_user_bri=200;           /* brillo elegido por el usuario (0-255) */
static const char *g_cfg_path="/tuya/data/custom/tpp06_ui.conf";
static void build_home(void);        /* fwd */
static void rebuild_home(void);      /* fwd */
static void open_rename(Item*it);    /* fwd (E1-T02) */

static char g_host[40]="", g_token[1200]=""; static int g_port=8123;

static void trim(char*s){
    char*e=s+strlen(s);
    while(e>s&&(e[-1]=='\n'||e[-1]=='\r'||e[-1]==' '||e[-1]=='\t')) *--e=0;
    while(*s==' '||*s=='\t') memmove(s,s+1,strlen(s));
}
static void load_config(const char*path){
    FILE*f=fopen(path,"r"); if(!f){ fprintf(stderr,"sin config %s\n",path); return; }
    char line[1400];
    while(fgets(line,sizeof line,f)){
        trim(line);
        if(line[0]=='#'||line[0]==0) continue;
        char*eq=strchr(line,'='); if(!eq) continue;
        *eq=0; char*key=line,*val=eq+1;
        if(!strcmp(key,"ha_host")) strncpy(g_host,val,sizeof g_host-1);
        else if(!strcmp(key,"ha_port")) g_port=atoi(val);
        else if(!strcmp(key,"ha_token")) strncpy(g_token,val,sizeof g_token-1);
        else if(!strcmp(key,"wallpaper")) g_wall=atoi(val);
        else if((!strcmp(key,"widget")||!strcmp(key,"relay")) && n_items<16){
            /* tokeniza val por '|' */
            char*f[6]={0}; int nf=0; { char*p=val; f[nf++]=p; while((p=strchr(p,'|'))&&nf<6){ *p++=0; f[nf++]=p; } }
            Item*it=&items[n_items]; memset(it,0,sizeof *it);
            it->state=-1; it->page=-1;
            if(!strcmp(key,"widget")){
                it->kind=0;
                strncpy(it->entity, f[0]?f[0]:"", HA_ID_LEN-1);
                strncpy(it->label, (f[1]&&f[1][0])?f[1]:it->entity, sizeof it->label-1);
                strncpy(it->domain, (f[2]&&f[2][0])?f[2]:"light", sizeof it->domain-1);
                if(f[3]&&f[3][0]) it->page=atoi(f[3]);
            } else {
                it->kind=1; it->relay=atoi(f[0]?f[0]:"1");
                snprintf(it->label,sizeof it->label,"Rele %d",it->relay);
                if(f[1]&&f[1][0]){ strncpy(it->label,f[1],sizeof it->label-1); it->label[sizeof it->label-1]=0; }
                if(f[2]&&f[2][0]) it->page=atoi(f[2]);
            }
            n_items++;
        }
    }
    fclose(f);
    /* config antigua sin páginas: auto-asigna (4 en la 1ª, 6 en las demás) */
    if(n_items>0 && items[0].page<0){
        int p=0,c=0;
        for(int i=0;i<n_items;i++){ items[i].page=p; c++; if(c>=(p==0?4:6)){ p++; c=0; } }
    }
    for(int i=0;i<n_items;i++) if(items[i].page<0) items[i].page=0;
}
/* guarda el layout actual (persiste la edición) */
static void save_config(void){
    char tmp[300]; snprintf(tmp,sizeof tmp,"%s.tmp",g_cfg_path);
    FILE*f=fopen(tmp,"w"); if(!f){ fprintf(stderr,"no puedo guardar config\n"); return; }
    fprintf(f,"ha_host=%s\n", g_host);
    fprintf(f,"ha_port=%d\n", g_port);
    if(g_token[0]) fprintf(f,"ha_token=%s\n", g_token);
    fprintf(f,"wallpaper=%d\n", g_wall);
    for(int i=0;i<n_items;i++){
        Item*it=&items[i];
        if(it->kind==1) fprintf(f,"relay=%d|%s|%d\n", it->relay, it->label, it->page);
        else fprintf(f,"widget=%s|%s|%s|%d\n", it->entity, it->label, it->domain[0]?it->domain:"light", it->page);
    }
    fclose(f);
    rename(tmp, g_cfg_path);
}
/* borra el item en idx (y persiste) */
static void del_item(int idx){
    if(idx<0||idx>=n_items) return;
    for(int i=idx;i<n_items-1;i++) items[i]=items[i+1];
    n_items--;
    save_config();
    rebuild_home();
}
/* mueve el item de la posición 'from' a 'to' en items[] (reordenar) */
static void move_item(int from,int to){
    if(from==to||from<0||to<0||from>=n_items||to>=n_items) return;
    Item tmp=items[from];
    if(from<to){ for(int i=from;i<to;i++) items[i]=items[i+1]; }
    else       { for(int i=from;i>to;i--) items[i]=items[i-1]; }
    items[to]=tmp;
}

/* --------------------------------- HA ------------------------------------ */
static ha_client ha;

static int is_cover(Item*it){ return it->kind==0 && !strcmp(it->domain,"cover"); }
static int state_is_on(const char*s){ return !strcmp(s,"on")||!strcmp(s,"open")||!strcmp(s,"opening"); }
/* icono según tipo (símbolos LVGL; fuente de iconos propia más adelante) */
static const char* item_icon(Item*it){
    if(it->kind==1) return LV_SYMBOL_POWER;
    if(!strcmp(it->domain,"cover")) return LV_SYMBOL_UP;
    if(!strcmp(it->domain,"switch")) return LV_SYMBOL_POWER;
    if(!strcmp(it->domain,"light")) return LV_SYMBOL_CHARGE;
    return LV_SYMBOL_POWER;
}

/* aspecto del tile según estado (1 on / 0 off / -1 n/d) */
static void tile_set_visual(Item*it, int on){
    if(!it->tile) return;
    lv_color_t bg, fg, ic;
    if(on==1){ bg=lv_color_hex(0xFFCF6B); fg=lv_color_hex(0x2a2410); ic=lv_color_hex(0x8a5a10); }
    else if(on==0){ bg=lv_color_hex(0x21232e); fg=lv_color_hex(0xdcdce4); ic=lv_color_hex(0x8a8d9a); }
    else { bg=lv_color_hex(0x191a22); fg=lv_color_hex(0x6a6d7a); ic=lv_color_hex(0x55596a); }
    lv_obj_set_style_bg_color(it->tile, bg, 0);
    if(it->name_lbl) lv_obj_set_style_text_color(it->name_lbl, fg, 0);
    if(it->icon) lv_obj_set_style_text_color(it->icon, ic, 0);
}
static void refresh_item(Item*it){
    int s;
    if(it->kind==1) s=relay_get(it->relay);
    else { ha_entity*e=ha_get(&ha,it->entity); s=(e&&e->known)?(state_is_on(e->state)?1:0):-1; }
    it->state=s;
    tile_set_visual(it, s);
}
static void refresh_all(void){ for(int i=0;i<n_items;i++) refresh_item(&items[i]); }

/* fija el estado (relé local o entidad HA) con feedback optimista */
static void do_set(Item*it, int on){
    if(it->kind==1) relay_set(it->relay,on);
    else if(is_cover(it)) ha_call(&ha,"cover", on?"open_cover":"close_cover", it->entity);
    else ha_call(&ha,it->domain,on?"turn_on":"turn_off",it->entity);
    it->state=on; tile_set_visual(it,on);
}
static void do_toggle(Item*it){ do_set(it, it->state==1?0:1); }

static void tile_short_cb(lv_event_t*e){ do_toggle(lv_event_get_user_data(e)); }

/* ------------------------- detalle (pantalla completa) ------------------- */
static lv_obj_t *scr_home=NULL;        /* pantalla de inicio persistente */
static Item *g_det_item=NULL;

/* fondos de pantalla (degradados) */
static const uint32_t WALL[][2] = {
    {0x0d0e15,0x1b2742}, {0x141414,0x141414}, {0x0b1e13,0x163a28},
    {0x1e0b1e,0x3a1636}, {0x1a1206,0x3a2a12}, {0x0a1420,0x123a52},
};
#define N_WALL 6
static void apply_bg(lv_obj_t*o){
    if(g_wall<0||g_wall>=N_WALL) g_wall=0;
    lv_obj_set_style_bg_color(o, lv_color_hex(WALL[g_wall][0]),0);
    lv_obj_set_style_bg_grad_color(o, lv_color_hex(WALL[g_wall][1]),0);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER,0);
}
static void detail_back_cb(lv_event_t*e){ (void)e;
    g_det_item=NULL;
    lv_scr_load_anim(scr_home, LV_SCR_LOAD_ANIM_NONE, 0, 0, true); /* borra la pantalla de detalle */
}
static void detail_sw_cb(lv_event_t*e){
    if(!g_det_item) return;
    int on=lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    do_set(g_det_item, on);
}
static void detail_slider_cb(lv_event_t*e){
    if(!g_det_item || g_det_item->kind!=0) return;
    int v=lv_slider_get_value(lv_event_get_target(e));
    ha_call_bri(&ha, g_det_item->entity, v);
}
static void cover_open_cb(lv_event_t*e){ (void)e; if(g_det_item) ha_call(&ha,"cover","open_cover",g_det_item->entity); }
static void cover_stop_cb(lv_event_t*e){ (void)e; if(g_det_item) ha_call(&ha,"cover","stop_cover",g_det_item->entity); }
static void cover_close_cb(lv_event_t*e){(void)e; if(g_det_item) ha_call(&ha,"cover","close_cover",g_det_item->entity); }
static void cover_pos_cb(lv_event_t*e){ if(g_det_item) ha_call_cover_pos(&ha,g_det_item->entity, lv_slider_get_value(lv_event_get_target(e))); }
static void open_detail(Item*it){
    g_det_item=it;
    lv_obj_t*scr=lv_obj_create(NULL);
    apply_bg(scr);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* botón atrás */
    lv_obj_t*bk=lv_btn_create(scr);
    lv_obj_set_size(bk, 58, 46);
    lv_obj_align(bk, LV_ALIGN_TOP_LEFT, 10, 12);
    lv_obj_set_style_bg_color(bk, lv_color_hex(0x262835),0);
    lv_obj_set_style_radius(bk, 14,0);
    lv_obj_add_event_cb(bk, detail_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t*bi=lv_label_create(bk); lv_label_set_text(bi, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(bi, lv_color_hex(0xffffff),0); lv_obj_center(bi);

    /* nombre grande */
    lv_obj_t*nm=lv_label_create(scr);
    lv_label_set_text(nm, it->label);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nm, SW-40);
    lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(nm, lv_color_hex(0xffffff),0);
    lv_obj_set_style_text_font(nm,&lv_font_montserrat_28,0);
    lv_obj_align(nm, LV_ALIGN_TOP_MID, 0, 74);

    if(is_cover(it)){
        /* covers: abrir / parar / cerrar */
        const char*ics[3]={LV_SYMBOL_UP,LV_SYMBOL_STOP,LV_SYMBOL_DOWN};
        lv_event_cb_t cbs[3]={cover_open_cb,cover_stop_cb,cover_close_cb};
        for(int i=0;i<3;i++){
            lv_obj_t*b=lv_btn_create(scr); lv_obj_set_size(b,82,66);
            lv_obj_align(b,LV_ALIGN_TOP_MID,(i-1)*94,150);
            lv_obj_set_style_bg_color(b,lv_color_hex(0x2a3550),0); lv_obj_set_style_radius(b,14,0);
            lv_obj_add_event_cb(b,cbs[i],LV_EVENT_CLICKED,NULL);
            lv_obj_t*l=lv_label_create(b); lv_label_set_text(l,ics[i]);
            lv_obj_set_style_text_font(l,&lv_font_montserrat_28,0); lv_obj_set_style_text_color(l,lv_color_hex(0xffffff),0); lv_obj_center(l);
        }
        /* slider de posición si el cover lo soporta */
        ha_entity*ce=ha_get(&ha,it->entity);
        if(ce && ce->position>=0){
            lv_obj_t*pl=lv_label_create(scr); lv_label_set_text(pl,"Posicion");
            lv_obj_set_style_text_color(pl,lv_color_hex(0xc3c3cd),0); lv_obj_set_style_text_font(pl,&lv_font_montserrat_16,0);
            lv_obj_align(pl,LV_ALIGN_TOP_LEFT,28,250);
            lv_obj_t*ps=lv_slider_create(scr); lv_obj_set_width(ps,SW-56); lv_obj_align(ps,LV_ALIGN_TOP_MID,0,284);
            lv_slider_set_range(ps,0,100); lv_slider_set_value(ps,ce->position,LV_ANIM_OFF);
            lv_obj_set_style_bg_color(ps,lv_color_hex(0x5aa0ff),LV_PART_INDICATOR);
            lv_obj_set_style_bg_color(ps,lv_color_hex(0x5aa0ff),LV_PART_KNOB);
            lv_obj_add_event_cb(ps,cover_pos_cb,LV_EVENT_VALUE_CHANGED,NULL);
        }
    } else {
        /* on/off grande */
        lv_obj_t*sw=lv_switch_create(scr);
        lv_obj_set_size(sw, 130, 64);
        lv_obj_align(sw, LV_ALIGN_TOP_MID, 0, 140);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x2bd25a), LV_PART_INDICATOR|LV_STATE_CHECKED);
        if(it->state==1) lv_obj_add_state(sw, LV_STATE_CHECKED);
        lv_obj_add_event_cb(sw, detail_sw_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    /* brillo (luces regulables) */
    ha_entity*e=(it->kind==0)? ha_get(&ha,it->entity):NULL;
    if(e && e->brightness>=0){
        lv_obj_t*bl=lv_label_create(scr);
        lv_label_set_text(bl,"Brillo");
        lv_obj_set_style_text_color(bl, lv_color_hex(0xc3c3cd),0);
        lv_obj_set_style_text_font(bl,&lv_font_montserrat_16,0);
        lv_obj_align(bl, LV_ALIGN_TOP_LEFT, 28, 250);
        lv_obj_t*sl=lv_slider_create(scr);
        lv_obj_set_width(sl, SW-56);
        lv_obj_align(sl, LV_ALIGN_TOP_MID, 0, 284);
        lv_slider_set_range(sl, 1, 100);
        lv_slider_set_value(sl, e->brightness*100/255, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFCF6B), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sl, lv_color_hex(0xFFCF6B), LV_PART_KNOB);
        lv_obj_add_event_cb(sl, detail_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false); /* sin animación; mantiene el home */
}
static void tile_long_cb(lv_event_t*e){ open_detail(lv_event_get_user_data(e)); }

/* --------------------------------- reloj --------------------------------- */
static lv_obj_t *lbl_time=NULL, *lbl_date=NULL;
static void clock_update(lv_timer_t*t){ (void)t;
    time_t now=time(NULL); struct tm tmv; localtime_r(&now,&tmv);
    char hhmm[8]; snprintf(hhmm,sizeof hhmm,"%02d:%02d", tmv.tm_hour, tmv.tm_min);
    if(lbl_time) lv_label_set_text(lbl_time, hhmm);
    static const char*dow[]={"Dom","Lun","Mar","Mie","Jue","Vie","Sab"};
    static const char*mon[]={"Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"};
    char d[48]; snprintf(d,sizeof d,"%s %d %s", dow[tmv.tm_wday%7], tmv.tm_mday, mon[tmv.tm_mon%12]);
    if(lbl_date) lv_label_set_text(lbl_date, d);
}

/* -------------------------------- un tile -------------------------------- */
static void del_badge_cb(lv_event_t*e){ del_item((int)((Item*)lv_event_get_user_data(e) - items)); }
static void tile_rename_cb(lv_event_t*e){ open_rename(lv_event_get_user_data(e)); }  /* E1-T02 */
/* arrastrar para reordenar (en modo edición) */
static Item *g_drag=NULL; static int g_drag_on=0; static lv_point_t g_drag_start;
static void tile_drag_cb(lv_event_t*e){
    lv_event_code_t c=lv_event_get_code(e);
    lv_obj_t*obj=lv_event_get_target(e);
    Item*it=lv_event_get_user_data(e);
    lv_indev_t*indev=lv_indev_get_act(); if(!indev) return;
    lv_point_t p; lv_indev_get_point(indev,&p);
    if(c==LV_EVENT_PRESSED){ g_drag=it; g_drag_on=0; g_drag_start=p; }
    else if(c==LV_EVENT_PRESSING){
        if(g_drag!=it) return;
        if(!g_drag_on && (abs(p.x-g_drag_start.x)>14 || abs(p.y-g_drag_start.y)>14)){
            g_drag_on=1; lv_obj_add_flag(obj,LV_OBJ_FLAG_FLOATING); lv_obj_move_foreground(obj);
        }
        if(g_drag_on){ lv_point_t v; lv_indev_get_vect(indev,&v);
            lv_obj_set_pos(obj, lv_obj_get_x(obj)+v.x, lv_obj_get_y(obj)+v.y); }
    }
    else if(c==LV_EVENT_RELEASED){
        if(g_drag==it && g_drag_on){
            int gfrom=(int)(it-items), arr[16], cnt=0, slot=-1;
            for(int i=0;i<n_items;i++) if(items[i].page==g_edit_page) arr[cnt++]=i;
            for(int s=0;s<cnt;s++){ lv_obj_t*t=items[arr[s]].tile; if(!t||t==obj) continue;
                lv_area_t a; lv_obj_get_coords(t,&a);
                if(p.x>=a.x1&&p.x<=a.x2&&p.y>=a.y1&&p.y<=a.y2){ slot=s; break; } }
            g_drag=NULL; g_drag_on=0;
            if(slot>=0){ move_item(gfrom, arr[slot]); save_config(); }
            rebuild_home();
            return;
        }
        g_drag=NULL; g_drag_on=0;
    }
}
static void create_tile(lv_obj_t*parent, Item*it){
    lv_obj_t *tile=lv_obj_create(parent);
    lv_obj_set_size(tile, 136, 110);
    lv_obj_set_style_radius(tile, 18, 0);
    lv_obj_set_style_border_width(tile,0,0);
    lv_obj_set_style_pad_all(tile,12,0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(tile, LV_OBJ_FLAG_CLICKABLE);
    it->tile=tile;
    lv_obj_t *ic=lv_label_create(tile);
    lv_label_set_text(ic, item_icon(it));
    lv_obj_set_style_text_font(ic,&lv_font_montserrat_28,0);
    lv_obj_align(ic, LV_ALIGN_TOP_LEFT, 0, 0);
    it->icon=ic;
    lv_obj_t *nm=lv_label_create(tile);
    lv_label_set_text(nm, it->label);
    lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
    lv_obj_set_width(nm, 112);
    lv_obj_set_style_text_font(nm,&lv_font_montserrat_16,0);
    lv_obj_align(nm, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    it->name_lbl=nm;
    if(g_edit){
        /* badge de borrar (×) */
        lv_obj_t*bd=lv_obj_create(tile);
        lv_obj_set_size(bd,28,28); lv_obj_align(bd,LV_ALIGN_TOP_RIGHT,8,-8);
        lv_obj_set_style_bg_color(bd,lv_color_hex(0xe0463c),0); lv_obj_set_style_radius(bd,14,0);
        lv_obj_set_style_border_width(bd,0,0); lv_obj_clear_flag(bd,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(bd,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t*x=lv_label_create(bd); lv_label_set_text(x,LV_SYMBOL_CLOSE);
        lv_obj_set_style_text_color(x,lv_color_hex(0xffffff),0); lv_obj_center(x);
        lv_obj_add_event_cb(bd,del_badge_cb,LV_EVENT_CLICKED,it);
        /* arrastrar para reordenar; toque corto = renombrar */
        lv_obj_add_event_cb(tile, tile_drag_cb, LV_EVENT_PRESSED, it);
        lv_obj_add_event_cb(tile, tile_drag_cb, LV_EVENT_PRESSING, it);
        lv_obj_add_event_cb(tile, tile_drag_cb, LV_EVENT_RELEASED, it);
        lv_obj_add_event_cb(tile, tile_rename_cb, LV_EVENT_SHORT_CLICKED, it);
    } else {
        lv_obj_add_event_cb(tile, tile_short_cb, LV_EVENT_SHORT_CLICKED, it);
        lv_obj_add_event_cb(tile, tile_long_cb, LV_EVENT_LONG_PRESSED, it);
    }
    tile_set_visual(it, it->state);
}

/* ----------------------------- ajustes ----------------------------------- */
static lv_obj_t *g_scr_settings=NULL;

static void get_ip(char*buf,int n){
    strncpy(buf,"-",n);
    int s=socket(AF_INET,SOCK_DGRAM,0); if(s<0) return;
    struct ifreq ifr; memset(&ifr,0,sizeof ifr);
    strncpy(ifr.ifr_name,"wlan0",IFNAMSIZ-1);
    if(ioctl(s,SIOCGIFADDR,&ifr)==0){
        struct sockaddr_in*sa=(struct sockaddr_in*)&ifr.ifr_addr;
        strncpy(buf, inet_ntoa(sa->sin_addr), n-1); buf[n-1]=0;
    }
    close(s);
}
/* back genérico: carga el screen destino y borra el actual */
static void nav_back_cb(lv_event_t*e){
    lv_scr_load_anim((lv_obj_t*)lv_event_get_user_data(e), LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}
static lv_obj_t* make_subscreen(const char*title, lv_obj_t*target){
    lv_obj_t*scr=lv_obj_create(NULL); apply_bg(scr); lv_obj_clear_flag(scr,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t*bk=lv_btn_create(scr); lv_obj_set_size(bk,58,46); lv_obj_align(bk,LV_ALIGN_TOP_LEFT,10,12);
    lv_obj_set_style_bg_color(bk,lv_color_hex(0x262835),0); lv_obj_set_style_radius(bk,14,0);
    lv_obj_add_event_cb(bk,nav_back_cb,LV_EVENT_CLICKED,target);
    lv_obj_t*bi=lv_label_create(bk); lv_label_set_text(bi,LV_SYMBOL_LEFT); lv_obj_set_style_text_color(bi,lv_color_hex(0xffffff),0); lv_obj_center(bi);
    lv_obj_t*ttl=lv_label_create(scr); lv_label_set_text(ttl,title);
    lv_obj_set_style_text_color(ttl,lv_color_hex(0xffffff),0); lv_obj_set_style_text_font(ttl,&lv_font_montserrat_20,0);
    lv_obj_align(ttl,LV_ALIGN_TOP_MID,0,20);
    return scr;
}
/* E1-T02: renombrar un tile (teclado en pantalla) */
static Item *g_rename_it=NULL; static lv_obj_t *g_scr_rename=NULL, *rn_ta=NULL;
static void rename_save_cb(lv_event_t*e){ (void)e;
    if(g_rename_it){
        strncpy(g_rename_it->label, lv_textarea_get_text(rn_ta), sizeof g_rename_it->label-1);
        g_rename_it->label[sizeof g_rename_it->label-1]=0;
        save_config();
    }
    lv_obj_t*old=g_scr_rename; g_scr_rename=NULL; g_rename_it=NULL;
    rebuild_home();
    if(old) lv_obj_del(old);
}
static void rename_ta_cb(lv_event_t*e){ if(lv_event_get_code(e)==LV_EVENT_READY) rename_save_cb(e); }
static void open_rename(Item*it){
    g_rename_it=it;
    lv_obj_t*scr=make_subscreen("Renombrar", scr_home); g_scr_rename=scr;
    rn_ta=lv_textarea_create(scr); lv_textarea_set_one_line(rn_ta,true);
    lv_textarea_set_text(rn_ta,it->label); lv_obj_set_width(rn_ta,SW-32); lv_obj_align(rn_ta,LV_ALIGN_TOP_MID,0,64);
    lv_obj_add_event_cb(rn_ta,rename_ta_cb,LV_EVENT_READY,NULL);
    lv_obj_t*sv=lv_btn_create(scr); lv_obj_set_size(sv,120,42); lv_obj_align(sv,LV_ALIGN_TOP_RIGHT,-12,12);
    lv_obj_set_style_bg_color(sv,lv_color_hex(0x2bd25a),0); lv_obj_set_style_radius(sv,21,0);
    lv_obj_add_event_cb(sv,rename_save_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t*svl=lv_label_create(sv); lv_label_set_text(svl,LV_SYMBOL_SAVE " Guardar"); lv_obj_set_style_text_color(svl,lv_color_hex(0x08240f),0); lv_obj_center(svl);
    lv_obj_t*kb=lv_keyboard_create(scr); lv_keyboard_set_textarea(kb,rn_ta);
    lv_scr_load_anim(scr,LV_SCR_LOAD_ANIM_NONE,0,0,false);
}
static void open_info(const char*title,const char*text){
    lv_obj_t*scr=make_subscreen(title, g_scr_settings);
    lv_obj_t*l=lv_label_create(scr); lv_label_set_text(l,text);
    lv_obj_set_width(l,SW-48); lv_label_set_long_mode(l,LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(l,lv_color_hex(0xc8c8d2),0); lv_obj_set_style_text_font(l,&lv_font_montserrat_16,0);
    lv_obj_align(l,LV_ALIGN_TOP_LEFT,24,80);
    lv_scr_load_anim(scr,LV_SCR_LOAD_ANIM_NONE,0,0,false);
}
/* --- brillo --- */
static void bri_slider_cb(lv_event_t*e){
    int v=lv_slider_get_value(lv_event_get_target(e)); int b=v*255/100; if(b<3)b=3;
    g_user_bri=b;
    char s[8]; int n=snprintf(s,sizeof s,"%d\n",b);
    int fd=open("/sys/class/backlight/backlight/brightness",O_WRONLY); if(fd>=0){ if(write(fd,s,n)<0){} close(fd);}
}
static int get_bri_pct(void){
    int fd=open("/sys/class/backlight/backlight/brightness",O_RDONLY); if(fd<0) return 60;
    char b[8]={0}; int r=read(fd,b,7); close(fd); if(r<=0) return 60; int v=atoi(b); return v*100/255;
}
static void open_brightness(void){
    lv_obj_t*scr=make_subscreen("Brillo", g_scr_settings);
    lv_obj_t*sl=lv_slider_create(scr); lv_obj_set_width(sl,SW-56); lv_obj_align(sl,LV_ALIGN_CENTER,0,-10);
    lv_slider_set_range(sl,5,100); lv_slider_set_value(sl,get_bri_pct(),LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl,lv_color_hex(0xFFCF6B),LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl,lv_color_hex(0xFFCF6B),LV_PART_KNOB);
    lv_obj_add_event_cb(sl,bri_slider_cb,LV_EVENT_VALUE_CHANGED,NULL);
    lv_scr_load_anim(scr,LV_SCR_LOAD_ANIM_NONE,0,0,false);
}
/* --- fondo de pantalla --- */
static void wall_cb(lv_event_t*e){
    g_wall=(int)(intptr_t)lv_event_get_user_data(e); save_config();
    apply_bg(lv_scr_act()); if(scr_home) apply_bg(scr_home);
}
static void open_wallpaper(void){
    lv_obj_t*scr=make_subscreen("Fondo de pantalla", g_scr_settings);
    lv_obj_t*cont=lv_obj_create(scr); lv_obj_set_size(cont,SW-24,SH-96); lv_obj_align(cont,LV_ALIGN_TOP_MID,0,72);
    lv_obj_set_style_bg_opa(cont,LV_OPA_0,0); lv_obj_set_style_border_width(cont,0,0);
    lv_obj_set_flex_flow(cont,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_all(cont,8,0); lv_obj_set_style_pad_row(cont,12,0); lv_obj_set_style_pad_column(cont,12,0);
    for(int i=0;i<N_WALL;i++){
        lv_obj_t*sw=lv_obj_create(cont); lv_obj_set_size(sw,86,86); lv_obj_set_style_radius(sw,14,0);
        lv_obj_set_style_border_width(sw, i==g_wall?3:0,0); lv_obj_set_style_border_color(sw,lv_color_hex(0xffffff),0);
        lv_obj_set_style_bg_color(sw,lv_color_hex(WALL[i][0]),0);
        lv_obj_set_style_bg_grad_color(sw,lv_color_hex(WALL[i][1]),0); lv_obj_set_style_bg_grad_dir(sw,LV_GRAD_DIR_VER,0);
        lv_obj_clear_flag(sw,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(sw,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(sw,wall_cb,LV_EVENT_CLICKED,(void*)(intptr_t)i);
    }
    lv_scr_load_anim(scr,LV_SCR_LOAD_ANIM_NONE,0,0,false);
}
/* --- acerca de / reiniciar / info --- */
static void open_about(void){
    char ip[32]; get_ip(ip,sizeof ip);
    long up=0; { int fd=open("/proc/uptime",O_RDONLY); char b[32]={0}; if(fd>=0){ if(read(fd,b,31)>0) up=atol(b); close(fd);} }
    char t[256]; snprintf(t,sizeof t,"TPP06 UI (LVGL)\nVersion 0.2\n\nIP: %s\nUptime: %ld s\nDispositivos: %d", ip, up, n_items);
    open_info("Acerca de", t);
}
static void do_reboot_cb(lv_event_t*e){ (void)e; sync(); system("reboot"); }
static void open_reboot(void){
    lv_obj_t*scr=make_subscreen("Reiniciar", g_scr_settings);
    lv_obj_t*l=lv_label_create(scr); lv_label_set_text(l,"Reiniciar el panel?");
    lv_obj_set_style_text_color(l,lv_color_hex(0xffffff),0); lv_obj_set_style_text_font(l,&lv_font_montserrat_20,0);
    lv_obj_align(l,LV_ALIGN_CENTER,0,-40);
    lv_obj_t*b=lv_btn_create(scr); lv_obj_set_size(b,190,56); lv_obj_align(b,LV_ALIGN_CENTER,0,30);
    lv_obj_set_style_bg_color(b,lv_color_hex(0xe0463c),0); lv_obj_set_style_radius(b,28,0);
    lv_obj_add_event_cb(b,do_reboot_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t*bl=lv_label_create(b); lv_label_set_text(bl,"Reiniciar ahora"); lv_obj_set_style_text_color(bl,lv_color_hex(0xffffff),0); lv_obj_center(bl);
    lv_scr_load_anim(scr,LV_SCR_LOAD_ANIM_NONE,0,0,false);
}
/* helpers de campo de texto + teclado (compartidos por HA/MQTT) */
static lv_obj_t *g_kb=NULL;
static void ha_ta_event(lv_event_t*e){
    lv_event_code_t c=lv_event_get_code(e);
    if(c==LV_EVENT_FOCUSED){ lv_keyboard_set_textarea(g_kb,lv_event_get_target(e)); lv_obj_clear_flag(g_kb,LV_OBJ_FLAG_HIDDEN); }
    else if(c==LV_EVENT_DEFOCUSED||c==LV_EVENT_READY||c==LV_EVENT_CANCEL){ lv_obj_add_flag(g_kb,LV_OBJ_FLAG_HIDDEN); }
}
static lv_obj_t* ha_field(lv_obj_t*scr,const char*lbl,const char*val,int y){
    lv_obj_t*l=lv_label_create(scr); lv_label_set_text(l,lbl);
    lv_obj_set_style_text_color(l,lv_color_hex(0xb9c2da),0); lv_obj_align(l,LV_ALIGN_TOP_LEFT,16,y);
    lv_obj_t*ta=lv_textarea_create(scr); lv_textarea_set_one_line(ta,true);
    lv_textarea_set_text(ta,val); lv_obj_set_width(ta,SW-32); lv_obj_align(ta,LV_ALIGN_TOP_LEFT,16,y+20);
    lv_obj_add_event_cb(ta,ha_ta_event,LV_EVENT_ALL,NULL);
    return ta;
}
static void open_wifi(void){
    char ip[32]; get_ip(ip,sizeof ip); char ssid[64]="(red actual)";
    FILE*p=popen("iwgetid -r 2>/dev/null","r"); if(p){ if(fgets(ssid,sizeof ssid,p)) ssid[strcspn(ssid,"\n")]=0; pclose(p);}
    char t[220]; snprintf(t,sizeof t,"Red: %s\nIP: %s\n\nLa configuracion de WiFi se anade proximamente (teclado en pantalla).", ssid, ip);
    open_info("WiFi", t);
}
/* E2-T02: editar MQTT del agente (tpp06.conf) */
#define MQTT_CONF "/tuya/data/custom/tpp06.conf"
static lv_obj_t *mq_host=NULL,*mq_port=NULL,*mq_user=NULL,*mq_pass=NULL;
static void mq_read(char*h,char*po,char*u,char*pa){
    h[0]=po[0]=u[0]=pa[0]=0; strcpy(po,"1883");
    FILE*f=fopen(MQTT_CONF,"r"); if(!f) return; char ln[256];
    while(fgets(ln,sizeof ln,f)){ ln[strcspn(ln,"\r\n")]=0; char*eq=strchr(ln,'='); if(!eq)continue; *eq=0; char*v=eq+1;
        if(!strcmp(ln,"broker_host")) strncpy(h,v,63);
        else if(!strcmp(ln,"broker_port")) strncpy(po,v,7);
        else if(!strcmp(ln,"broker_user")) strncpy(u,v,63);
        else if(!strcmp(ln,"broker_pass")) strncpy(pa,v,63);
    } fclose(f);
}
static void mq_save_cb(lv_event_t*e){ (void)e;
    char tmp[]="/tuya/data/custom/tpp06.conf.tmp"; FILE*f=fopen(tmp,"w"); if(f){
        fprintf(f,"broker_host=%s\n",lv_textarea_get_text(mq_host));
        fprintf(f,"broker_port=%s\n",lv_textarea_get_text(mq_port));
        fprintf(f,"broker_user=%s\n",lv_textarea_get_text(mq_user));
        fprintf(f,"broker_pass=%s\n",lv_textarea_get_text(mq_pass));
        fprintf(f,"telemetry_s=30\n"); fclose(f); rename(tmp,MQTT_CONF);
    }
    lv_scr_load_anim(g_scr_settings,LV_SCR_LOAD_ANIM_NONE,0,0,true);
}
static void open_mqtt(void){
    static char h[64],po[8],u[64],pa[64]; mq_read(h,po,u,pa);
    lv_obj_t*scr=make_subscreen("MQTT", g_scr_settings);
    lv_obj_t*sv=lv_btn_create(scr); lv_obj_set_size(sv,120,42); lv_obj_align(sv,LV_ALIGN_TOP_RIGHT,-12,12);
    lv_obj_set_style_bg_color(sv,lv_color_hex(0x2bd25a),0); lv_obj_set_style_radius(sv,21,0);
    lv_obj_add_event_cb(sv,mq_save_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t*svl=lv_label_create(sv); lv_label_set_text(svl,LV_SYMBOL_SAVE " Guardar"); lv_obj_set_style_text_color(svl,lv_color_hex(0x08240f),0); lv_obj_center(svl);
    mq_host=ha_field(scr,"Broker",h,60); mq_port=ha_field(scr,"Puerto",po,112);
    mq_user=ha_field(scr,"Usuario",u,164); mq_pass=ha_field(scr,"Contrasena",pa,216);
    g_kb=lv_keyboard_create(scr); lv_obj_add_flag(g_kb,LV_OBJ_FLAG_HIDDEN);
    lv_scr_load_anim(scr,LV_SCR_LOAD_ANIM_NONE,0,0,false);
}
static lv_obj_t *ta_host=NULL,*ta_port=NULL,*ta_token=NULL;
static void ha_save_cb(lv_event_t*e){ (void)e;
    strncpy(g_host,lv_textarea_get_text(ta_host),sizeof g_host-1); g_host[sizeof g_host-1]=0;
    g_port=atoi(lv_textarea_get_text(ta_port));
    strncpy(g_token,lv_textarea_get_text(ta_token),sizeof g_token-1); g_token[sizeof g_token-1]=0;
    save_config(); ha_close(&ha);
    lv_scr_load_anim(g_scr_settings, LV_SCR_LOAD_ANIM_NONE, 0, 0, true);
}
static void open_ha(void){
    lv_obj_t*scr=make_subscreen("Home Assistant", g_scr_settings);
    char ports[8]; snprintf(ports,sizeof ports,"%d",g_port);
    lv_obj_t*sv=lv_btn_create(scr); lv_obj_set_size(sv,120,42); lv_obj_align(sv,LV_ALIGN_TOP_RIGHT,-12,12);
    lv_obj_set_style_bg_color(sv,lv_color_hex(0x2bd25a),0); lv_obj_set_style_radius(sv,21,0);
    lv_obj_add_event_cb(sv,ha_save_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t*svl=lv_label_create(sv); lv_label_set_text(svl,LV_SYMBOL_SAVE " Guardar"); lv_obj_set_style_text_color(svl,lv_color_hex(0x08240f),0); lv_obj_center(svl);
    ta_host=ha_field(scr,"Servidor",g_host,64);
    ta_port=ha_field(scr,"Puerto",ports,118);
    ta_token=ha_field(scr,"Token",g_token,172);
    g_kb=lv_keyboard_create(scr); lv_obj_add_flag(g_kb,LV_OBJ_FLAG_HIDDEN);
    lv_scr_load_anim(scr,LV_SCR_LOAD_ANIM_NONE,0,0,false);
}
static void open_datetime(void){ time_t n=time(NULL); struct tm tmv; localtime_r(&n,&tmv); char t[160]; snprintf(t,sizeof t,"Hora actual:\n%02d:%02d:%02d\n\n(Sincronizacion automatica)", tmv.tm_hour,tmv.tm_min,tmv.tm_sec); open_info("Fecha y hora", t); }

typedef void(*openfn)(void);
static void row_cb(lv_event_t*e){ openfn f=(openfn)lv_event_get_user_data(e); if(f) f(); }
static void add_row(lv_obj_t*list,const char*icon,const char*text,openfn f){
    lv_obj_t*b=lv_list_add_btn(list, icon, text);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x21232e),0);
    lv_obj_set_style_text_color(b,lv_color_hex(0xe8e8ee),0);
    lv_obj_set_style_border_width(b,0,0); lv_obj_set_style_radius(b,12,0);
    lv_obj_set_style_pad_ver(b,20,0);
    lv_obj_set_style_text_font(b,&lv_font_montserrat_20,0);
    lv_obj_add_event_cb(b,row_cb,LV_EVENT_CLICKED,(void*)f);
}
static void open_settings(void){
    lv_obj_t*scr=lv_obj_create(NULL); apply_bg(scr); lv_obj_clear_flag(scr,LV_OBJ_FLAG_SCROLLABLE);
    g_scr_settings=scr;
    lv_obj_t*bk=lv_btn_create(scr); lv_obj_set_size(bk,58,46); lv_obj_align(bk,LV_ALIGN_TOP_LEFT,10,12);
    lv_obj_set_style_bg_color(bk,lv_color_hex(0x262835),0); lv_obj_set_style_radius(bk,14,0);
    lv_obj_add_event_cb(bk,nav_back_cb,LV_EVENT_CLICKED,scr_home);
    lv_obj_t*bi=lv_label_create(bk); lv_label_set_text(bi,LV_SYMBOL_LEFT); lv_obj_set_style_text_color(bi,lv_color_hex(0xffffff),0); lv_obj_center(bi);
    lv_obj_t*ttl=lv_label_create(scr); lv_label_set_text(ttl,"Ajustes");
    lv_obj_set_style_text_color(ttl,lv_color_hex(0xffffff),0); lv_obj_set_style_text_font(ttl,&lv_font_montserrat_28,0);
    lv_obj_align(ttl,LV_ALIGN_TOP_MID,0,20);
    lv_obj_t*list=lv_list_create(scr);
    lv_obj_set_size(list, SW-16, SH-76); lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 66);
    lv_obj_set_style_bg_opa(list,LV_OPA_0,0); lv_obj_set_style_border_width(list,0,0);
    lv_obj_set_style_pad_row(list,8,0);
    add_row(list, LV_SYMBOL_WIFI, "WiFi", open_wifi);
    add_row(list, LV_SYMBOL_IMAGE, "Fondo de pantalla", open_wallpaper);
    add_row(list, LV_SYMBOL_EYE_OPEN, "Brillo", open_brightness);
    add_row(list, LV_SYMBOL_UPLOAD, "MQTT", open_mqtt);
    add_row(list, LV_SYMBOL_HOME, "Home Assistant", open_ha);
    add_row(list, LV_SYMBOL_BELL, "Fecha y hora", open_datetime);
    add_row(list, LV_SYMBOL_LIST, "Acerca de", open_about);
    add_row(list, LV_SYMBOL_POWER, "Reiniciar", open_reboot);
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/* ------------------------- centro de control ----------------------------- */
static lv_obj_t *cc_panel=NULL, *cc_bd=NULL; static int cc_open=0;
static lv_obj_t *g_tileview=NULL, *g_dots=NULL; static int g_npages=1;
static void cc_anim_y(void*o,int32_t v){ lv_obj_set_y((lv_obj_t*)o,v); }
static void cc_show(int show){
    if(!cc_panel) return; cc_open=show;
    if(show){ lv_obj_clear_flag(cc_bd,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(cc_panel,LV_OBJ_FLAG_HIDDEN); }
    else { lv_obj_add_flag(cc_bd,LV_OBJ_FLAG_HIDDEN); }
    lv_anim_t a; lv_anim_init(&a); lv_anim_set_var(&a,cc_panel);
    lv_anim_set_values(&a, lv_obj_get_y(cc_panel), show?0:-180);
    lv_anim_set_time(&a,220); lv_anim_set_exec_cb(&a,cc_anim_y);
    lv_anim_start(&a);
}
static void cc_bd_cb(lv_event_t*e){ (void)e; cc_show(0); }
static void cc_settings_cb(lv_event_t*e){ (void)e; cc_show(0); open_settings(); }
static void cc_edit_cb(lv_event_t*e){ (void)e; g_edit_page=g_cur_page; g_edit=1; rebuild_home(); }
static void done_cb(lv_event_t*e){ (void)e; g_edit=0; g_cur_page=g_edit_page; rebuild_home(); }
static void build_control_center(void){
    cc_open=0;   /* fresco al (re)construir: evita que el gesto quede bloqueado */
    cc_bd=lv_obj_create(scr_home); lv_obj_set_size(cc_bd,SW,SH); lv_obj_set_pos(cc_bd,0,0);
    lv_obj_set_style_bg_color(cc_bd,lv_color_hex(0x000000),0); lv_obj_set_style_bg_opa(cc_bd,LV_OPA_40,0);
    lv_obj_set_style_border_width(cc_bd,0,0); lv_obj_set_style_radius(cc_bd,0,0);
    lv_obj_clear_flag(cc_bd,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(cc_bd,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cc_bd,cc_bd_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_add_flag(cc_bd,LV_OBJ_FLAG_HIDDEN);

    cc_panel=lv_obj_create(scr_home); lv_obj_set_size(cc_panel,SW,180); lv_obj_set_pos(cc_panel,0,-180);
    lv_obj_set_style_bg_color(cc_panel,lv_color_hex(0x15171f),0);
    lv_obj_set_style_radius(cc_panel,0,0); lv_obj_set_style_border_width(cc_panel,0,0);
    lv_obj_clear_flag(cc_panel,LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t*bs=lv_btn_create(cc_panel); lv_obj_set_size(bs,120,94); lv_obj_align(bs,LV_ALIGN_CENTER,-66,-4);
    lv_obj_set_style_bg_color(bs,lv_color_hex(0x262835),0); lv_obj_set_style_radius(bs,16,0);
    lv_obj_add_event_cb(bs,cc_settings_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t*bsl=lv_label_create(bs); lv_label_set_text(bsl,LV_SYMBOL_SETTINGS"\nAjustes");
    lv_obj_set_style_text_align(bsl,LV_TEXT_ALIGN_CENTER,0); lv_obj_set_style_text_color(bsl,lv_color_hex(0xffffff),0); lv_obj_center(bsl);

    lv_obj_t*be=lv_btn_create(cc_panel); lv_obj_set_size(be,120,94); lv_obj_align(be,LV_ALIGN_CENTER,66,-4);
    lv_obj_set_style_bg_color(be,lv_color_hex(0x262835),0); lv_obj_set_style_radius(be,16,0);
    lv_obj_add_event_cb(be,cc_edit_cb,LV_EVENT_CLICKED,NULL);
    lv_obj_t*bel=lv_label_create(be); lv_label_set_text(bel,LV_SYMBOL_EDIT"\nEditar");
    lv_obj_set_style_text_align(bel,LV_TEXT_ALIGN_CENTER,0); lv_obj_set_style_text_color(bel,lv_color_hex(0xffffff),0); lv_obj_center(bel);

    lv_obj_t*grip=lv_obj_create(cc_panel); lv_obj_set_size(grip,48,5); lv_obj_align(grip,LV_ALIGN_BOTTOM_MID,0,-6);
    lv_obj_set_style_bg_color(grip,lv_color_hex(0x55596a),0); lv_obj_set_style_radius(grip,3,0); lv_obj_set_style_border_width(grip,0,0);
    lv_obj_add_flag(cc_panel,LV_OBJ_FLAG_HIDDEN);
}
static int strip_sy=0;
static void strip_cb(lv_event_t*e){
    lv_event_code_t c=lv_event_get_code(e);
    lv_point_t p; lv_indev_get_point(lv_indev_get_act(), &p);
    if(c==LV_EVENT_PRESSED) strip_sy=p.y;
    else if(c==LV_EVENT_PRESSING){ if(!cc_open && p.y-strip_sy>26) cc_show(1); }
}

/* ------------------------------ dots páginas ----------------------------- */
static void update_dots(int active){
    if(!g_dots) return;
    uint32_t cnt=lv_obj_get_child_cnt(g_dots);
    for(uint32_t i=0;i<cnt;i++)
        lv_obj_set_style_bg_color(lv_obj_get_child(g_dots,i),
            (int)i==active?lv_color_hex(0xffffff):lv_color_hex(0x55596a),0);
}
static void tv_changed_cb(lv_event_t*e){
    lv_obj_t*tv=lv_event_get_target(e);
    int px = SW? (lv_obj_get_scroll_x(tv)+SW/2)/SW : 0;
    g_cur_page=px;
    update_dots(px);
}

/* reloj como bloque (ocupa toda la fila) en la primera página */
static void add_clock_widget(lv_obj_t*page){
    lv_obj_t*cw=lv_obj_create(page);
    lv_obj_set_size(cw, lv_pct(100), 150);
    lv_obj_set_style_bg_opa(cw, LV_OPA_0, 0);
    lv_obj_set_style_border_width(cw,0,0);
    lv_obj_set_style_pad_all(cw,0,0);
    lv_obj_clear_flag(cw, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(cw, LV_OBJ_FLAG_CLICKABLE);
    lbl_time=lv_label_create(cw);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(0xffffff),0);
    lv_obj_set_style_text_font(lbl_time,&lv_font_montserrat_48,0);
    lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, -16);
    lbl_date=lv_label_create(cw);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(0xb9c2da),0);
    lv_obj_set_style_text_font(lbl_date,&lv_font_montserrat_20,0);
    lv_obj_align(lbl_date, LV_ALIGN_CENTER, 0, 36);
}

/* ------------------------------- catálogo (wizard) ----------------------- */
static lv_obj_t *g_scr_wizard=NULL, *g_scr_entlist=NULL;
/* POST /api/template a HA para listar entidades de un dominio (ligero) */
static int http_post_template(const char*domain, char*out, int outcap){
    out[0]=0;
    if(!g_host[0]||!g_token[0]) return -1;
    int fd=socket(AF_INET,SOCK_STREAM,0); if(fd<0) return -1;
    struct sockaddr_in sa; memset(&sa,0,sizeof sa); sa.sin_family=AF_INET; sa.sin_port=htons(g_port);
    if(!inet_aton(g_host,&sa.sin_addr)){ close(fd); return -1; }
    struct timeval tv={6,0};
    setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof tv);
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof tv);
    if(connect(fd,(struct sockaddr*)&sa,sizeof sa)<0){ close(fd); return -1; }
    char body[256]; int bl=snprintf(body,sizeof body,
        "{\"template\":\"{%% for s in states.%s %%}{{s.entity_id}}~{{s.name}}\\n{%% endfor %%}\"}", domain);
    char req[1200]; int rl=snprintf(req,sizeof req,
        "POST /api/template HTTP/1.0\r\nHost: %s:%d\r\nAuthorization: Bearer %s\r\n"
        "Content-Type: application/json\r\nContent-Length: %d\r\n\r\n%s",
        g_host,g_port,g_token,bl,body);
    { int off=0; while(off<rl){ int w=write(fd,req+off,rl-off); if(w<=0) break; off+=w; } }
    static char buf[16384]; int total=0,n;
    while((n=read(fd,buf+total,sizeof buf-1-total))>0){ total+=n; if(total>=(int)sizeof buf-1) break; }
    close(fd); buf[total]=0;
    char*b=strstr(buf,"\r\n\r\n"); if(!b) return -1; b+=4;
    strncpy(out,b,outcap-1); out[outcap-1]=0;
    return 0;
}
static int add_item_internal(int kind,int relay,const char*entity,const char*domain,const char*label){
    if(n_items>=16) return -1;
    Item*it=&items[n_items]; memset(it,0,sizeof *it);
    it->kind=kind; it->relay=relay; it->page=g_edit_page; it->state=-1;
    strncpy(it->entity,entity?entity:"",HA_ID_LEN-1);
    strncpy(it->domain,domain?domain:"",sizeof it->domain-1);
    strncpy(it->label,label?label:"",sizeof it->label-1);
    n_items++;
    if(kind==0 && entity && entity[0]) ha_watch(&ha, entity);
    return 0;
}
typedef struct { int kind, relay; char entity[HA_ID_LEN], domain[16], label[32]; } PickData;
static PickData g_picks[160]; static int g_npicks=0;
/* seleccionar una entidad: añadir + guardar + volver al home de edición */
static void catalog_pick_cb(lv_event_t*e){
    int i=(int)(intptr_t)lv_event_get_user_data(e);
    if(i<0||i>=g_npicks) return;
    PickData*p=&g_picks[i];
    add_item_internal(p->kind,p->relay,p->entity,p->domain,p->label);
    save_config();
    if(p->kind==0) ha_close(&ha);            /* fuerza reconexión -> re-suscribe */
    lv_obj_t*a=g_scr_entlist, *b=g_scr_wizard; g_scr_entlist=NULL; g_scr_wizard=NULL;
    rebuild_home();
    if(a) lv_obj_del(a); if(b && b!=a) lv_obj_del(b);
}
/* fila grande y pulsable para la lista de entidades */
static void catalog_add_row(lv_obj_t*list,const char*icon,const char*text,
                            int kind,int relay,const char*entity,const char*domain,const char*label){
    if(g_npicks>=160) return;
    PickData*p=&g_picks[g_npicks]; memset(p,0,sizeof *p);
    p->kind=kind; p->relay=relay;
    strncpy(p->entity,entity?entity:"",HA_ID_LEN-1);
    strncpy(p->domain,domain?domain:"",sizeof p->domain-1);
    strncpy(p->label,(label&&label[0])?label:text,sizeof p->label-1);
    lv_obj_t*b=lv_list_add_btn(list, icon, text);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x21232e),0);
    lv_obj_set_style_text_color(b,lv_color_hex(0xf0f0f4),0);
    lv_obj_set_style_border_width(b,0,0); lv_obj_set_style_radius(b,12,0);
    lv_obj_set_style_pad_ver(b,22,0);
    lv_obj_set_style_text_font(b,&lv_font_montserrat_20,0);
    lv_obj_add_event_cb(b, catalog_pick_cb, LV_EVENT_CLICKED, (void*)(intptr_t)g_npicks);
    g_npicks++;
}
static void catalog_add_ha(lv_obj_t*list,const char*domain,const char*icon){
    static char out[16384];
    if(http_post_template(domain,out,sizeof out)!=0 || !out[0]){
        lv_obj_t*t=lv_list_add_text(list,"  (no se pudieron cargar)");
        lv_obj_set_style_text_color(t,lv_color_hex(0x8a8d9a),0); return;
    }
    char*p=out;
    while(*p){
        char*nl=strchr(p,'\n'); if(nl)*nl=0;
        char*tilde=strchr(p,'~');
        if(tilde && p[0]){ *tilde=0; const char*ent=p,*name=tilde+1;
            if(strncmp(ent,domain,strlen(domain))==0)
                catalog_add_row(list,icon,name[0]?name:ent,0,0,ent,domain,name[0]?name:ent);
        }
        if(!nl) break; p=nl+1;
    }
}
/* paso 2: lista de entidades del tipo elegido */
static void open_entity_list(const char*title,int kind,const char*domain,const char*icon){
    g_npicks=0;
    lv_obj_t*scr=make_subscreen(title, g_scr_wizard); g_scr_entlist=scr;
    lv_obj_t*list=lv_list_create(scr);
    lv_obj_set_size(list, SW-12, SH-72); lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_style_bg_opa(list,LV_OPA_0,0); lv_obj_set_style_border_width(list,0,0);
    lv_obj_set_style_pad_row(list,8,0);
    if(kind==1){
        for(int r=1;r<=3;r++){ char lbl[16]; snprintf(lbl,sizeof lbl,"Rele %d",r);
            catalog_add_row(list,LV_SYMBOL_POWER,lbl,1,r,"","",lbl); }
    } else {
        catalog_add_ha(list, domain, icon);
    }
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}
/* paso 1: wizard con 4 tiles de tipo */
static void wiz_reles_cb(lv_event_t*e){ (void)e; open_entity_list("Reles",1,"",LV_SYMBOL_POWER); }
static void wiz_luces_cb(lv_event_t*e){ (void)e; open_entity_list("Luces",0,"light",LV_SYMBOL_CHARGE); }
static void wiz_covers_cb(lv_event_t*e){ (void)e; open_entity_list("Covers",0,"cover",LV_SYMBOL_UP); }
static void wiz_switch_cb(lv_event_t*e){ (void)e; open_entity_list("Interruptores",0,"switch",LV_SYMBOL_SETTINGS); }
static void wiz_tile(lv_obj_t*parent,const char*icon,const char*text,lv_event_cb_t cb){
    lv_obj_t*t=lv_obj_create(parent); lv_obj_set_size(t,132,132); lv_obj_set_style_radius(t,18,0);
    lv_obj_set_style_bg_color(t,lv_color_hex(0x21232e),0); lv_obj_set_style_border_width(t,0,0);
    lv_obj_clear_flag(t,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(t,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t*ic=lv_label_create(t); lv_label_set_text(ic,icon);
    lv_obj_set_style_text_font(ic,&lv_font_montserrat_40,0); lv_obj_set_style_text_color(ic,lv_color_hex(0xc3c7d8),0);
    lv_obj_align(ic,LV_ALIGN_CENTER,0,-18);
    lv_obj_t*l=lv_label_create(t); lv_label_set_text(l,text);
    lv_obj_set_style_text_font(l,&lv_font_montserrat_16,0); lv_obj_set_style_text_color(l,lv_color_hex(0xe8e8ee),0);
    lv_obj_align(l,LV_ALIGN_CENTER,0,30);
    lv_obj_add_event_cb(t,cb,LV_EVENT_CLICKED,NULL);
}
static void open_catalog(void){
    lv_obj_t*scr=make_subscreen("Anadir", scr_home); g_scr_wizard=scr; g_scr_entlist=NULL;
    lv_obj_t*sub=lv_label_create(scr); lv_label_set_text(sub,"Que quieres anadir?");
    lv_obj_set_style_text_color(sub,lv_color_hex(0xb9c2da),0); lv_obj_set_style_text_font(sub,&lv_font_montserrat_16,0);
    lv_obj_align(sub,LV_ALIGN_TOP_MID,0,64);
    lv_obj_t*grid=lv_obj_create(scr); lv_obj_set_size(grid,SW-24,SH-110); lv_obj_align(grid,LV_ALIGN_TOP_MID,0,94);
    lv_obj_set_style_bg_opa(grid,LV_OPA_0,0); lv_obj_set_style_border_width(grid,0,0);
    lv_obj_set_flex_flow(grid,LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(grid,16,0); lv_obj_set_style_pad_column(grid,16,0);
    lv_obj_clear_flag(grid,LV_OBJ_FLAG_SCROLLABLE);
    wiz_tile(grid,LV_SYMBOL_POWER,"Reles",wiz_reles_cb);
    wiz_tile(grid,LV_SYMBOL_CHARGE,"Luces",wiz_luces_cb);
    wiz_tile(grid,LV_SYMBOL_UP,"Covers",wiz_covers_cb);
    wiz_tile(grid,LV_SYMBOL_SETTINGS,"Interruptores",wiz_switch_cb);
    lv_scr_load_anim(scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
}

/* ------------------------------ pantalla --------------------------------- */
static void add_tile_cb(lv_event_t*e){ (void)e; open_catalog(); }
static void build_home(void){
    scr_home = lv_obj_create(NULL);
    apply_bg(scr_home);
    lv_obj_clear_flag(scr_home, LV_OBJ_FLAG_SCROLLABLE);
    lbl_time=NULL; lbl_date=NULL;
    for(int i=0;i<n_items;i++){ items[i].tile=NULL; items[i].icon=NULL; items[i].name_lbl=NULL; }

    if(g_edit){
        /* cabecera de edición (solo la página g_edit_page) */
        char h[32]; snprintf(h,sizeof h,"Editar " LV_SYMBOL_DOWNLOAD " pag %d", g_edit_page+1);
        lv_obj_t*el=lv_label_create(scr_home); lv_label_set_text(el,h);
        lv_obj_set_style_text_color(el,lv_color_hex(0xffffff),0);
        lv_obj_set_style_text_font(el,&lv_font_montserrat_20,0);
        lv_obj_align(el, LV_ALIGN_TOP_LEFT, 14, 18);
        lv_obj_t*done=lv_btn_create(scr_home);
        lv_obj_set_size(done, 96, 42); lv_obj_align(done, LV_ALIGN_TOP_RIGHT, -12, 10);
        lv_obj_set_style_bg_color(done, lv_color_hex(0x2bd25a),0); lv_obj_set_style_radius(done,21,0);
        lv_obj_add_event_cb(done, done_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t*dl=lv_label_create(done); lv_label_set_text(dl, LV_SYMBOL_OK " Hecho");
        lv_obj_set_style_text_color(dl,lv_color_hex(0x08240f),0); lv_obj_center(dl);

        /* página única editable */
        lv_obj_t*page=lv_obj_create(scr_home);
        lv_obj_set_size(page, SW, SH-60); lv_obj_align(page, LV_ALIGN_TOP_MID, 0, 60);
        lv_obj_set_style_bg_opa(page,LV_OPA_0,0); lv_obj_set_style_border_width(page,0,0);
        lv_obj_set_style_pad_all(page,12,0); lv_obj_set_style_pad_row(page,12,0); lv_obj_set_style_pad_column(page,12,0);
        lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);   /* sin scroll: permite arrastrar tiles */
        for(int i=0;i<n_items;i++) if(items[i].page==g_edit_page) create_tile(page,&items[i]);
        /* tile '+' para añadir */
        lv_obj_t*addt=lv_obj_create(page); lv_obj_set_size(addt,136,110); lv_obj_set_style_radius(addt,18,0);
        lv_obj_set_style_border_width(addt,2,0); lv_obj_set_style_border_color(addt,lv_color_hex(0x55596a),0);
        lv_obj_set_style_bg_opa(addt,LV_OPA_0,0);
        lv_obj_clear_flag(addt,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(addt,LV_OBJ_FLAG_CLICKABLE);
        lv_obj_t*pl=lv_label_create(addt); lv_label_set_text(pl,LV_SYMBOL_PLUS);
        lv_obj_set_style_text_font(pl,&lv_font_montserrat_28,0); lv_obj_set_style_text_color(pl,lv_color_hex(0x9aa0b4),0); lv_obj_center(pl);
        lv_obj_add_event_cb(addt, add_tile_cb, LV_EVENT_CLICKED, NULL);
        g_npages=1;
    } else {
        /* páginas agrupadas por campo 'page'; el reloj va en la página 0 */
        int maxp=0; for(int i=0;i<n_items;i++) if(items[i].page>maxp) maxp=items[i].page;
        g_npages = (n_items>0)? maxp+1 : 1;
        lv_obj_t*tv=lv_tileview_create(scr_home); g_tileview=tv;
        lv_obj_set_size(tv, SW, SH-6-28); lv_obj_align(tv, LV_ALIGN_TOP_MID, 0, 6);
        lv_obj_set_style_bg_opa(tv, LV_OPA_0, 0); lv_obj_set_style_border_width(tv,0,0);
        lv_obj_set_scrollbar_mode(tv, LV_SCROLLBAR_MODE_OFF);
        for(int p=0;p<g_npages;p++){
            lv_obj_t*page=lv_tileview_add_tile(tv, p, 0, LV_DIR_HOR);
            lv_obj_set_style_pad_all(page,12,0); lv_obj_set_style_pad_row(page,12,0); lv_obj_set_style_pad_column(page,12,0);
            lv_obj_set_flex_flow(page, LV_FLEX_FLOW_ROW_WRAP);
            lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);   /* solo scroll horizontal entre páginas */
            if(p==0) add_clock_widget(page);
            for(int i=0;i<n_items;i++) if(items[i].page==p) create_tile(page, &items[i]);
        }
        lv_obj_add_event_cb(tv, tv_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
        if(g_cur_page>0 && g_cur_page<g_npages) lv_obj_set_tile_id(tv, g_cur_page, 0, LV_ANIM_OFF);

        /* dots de página */
        g_dots=lv_obj_create(scr_home);
        lv_obj_set_size(g_dots, SW, 24); lv_obj_align(g_dots, LV_ALIGN_BOTTOM_MID, 0, -4);
        lv_obj_set_style_bg_opa(g_dots,LV_OPA_0,0); lv_obj_set_style_border_width(g_dots,0,0);
        lv_obj_set_flex_flow(g_dots, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(g_dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(g_dots,8,0); lv_obj_clear_flag(g_dots,LV_OBJ_FLAG_SCROLLABLE);
        for(int p=0;p<g_npages;p++){
            lv_obj_t*d=lv_obj_create(g_dots); lv_obj_set_size(d,8,8); lv_obj_set_style_radius(d,4,0);
            lv_obj_set_style_border_width(d,0,0); lv_obj_clear_flag(d,LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(d, p==g_cur_page?lv_color_hex(0xffffff):lv_color_hex(0x55596a),0);
        }
        if(g_npages<=1) lv_obj_add_flag(g_dots, LV_OBJ_FLAG_HIDDEN);
    }

    /* franja superior: deslizar hacia abajo abre el centro de control */
    lv_obj_t*strip=lv_obj_create(scr_home); lv_obj_set_size(strip,SW,24); lv_obj_set_pos(strip,0,0);
    lv_obj_set_style_bg_opa(strip,LV_OPA_0,0); lv_obj_set_style_border_width(strip,0,0);
    lv_obj_clear_flag(strip,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(strip,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(strip, strip_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(strip, strip_cb, LV_EVENT_PRESSING, NULL);
    /* pista visual del tirador */
    lv_obj_t*hint=lv_obj_create(scr_home); lv_obj_set_size(hint,40,5); lv_obj_align(hint,LV_ALIGN_TOP_MID,0,6);
    lv_obj_set_style_bg_color(hint,lv_color_hex(0x4a4e60),0); lv_obj_set_style_radius(hint,3,0);
    lv_obj_set_style_border_width(hint,0,0); lv_obj_clear_flag(hint,LV_OBJ_FLAG_CLICKABLE);

    build_control_center();

    clock_update(NULL);
    lv_scr_load(scr_home);
}
static void rebuild_home(void){
    lv_obj_t*old=scr_home;
    build_home();        /* crea y carga el nuevo scr_home */
    refresh_all();       /* restaura el aspecto de los tiles desde su estado */
    if(old) lv_obj_del(old);
}

/* ---------------------- inactividad / salvapantallas --------------------- */
static lv_obj_t *g_scr_saver=NULL, *ss_time=NULL, *ss_date=NULL;
static int g_idle_state=0;   /* 0 activo, 1 salvapantallas, 2 apagado */
#define IDLE_SAVER_MS 40000
#define IDLE_OFF_MS   90000
static void bl_w(const char*node,const char*val){
    char p[80]; snprintf(p,sizeof p,"/sys/class/backlight/backlight/%s",node);
    int fd=open(p,O_WRONLY); if(fd>=0){ if(write(fd,val,strlen(val))<0){} close(fd);} }
static void bl_on(void){ bl_w("bl_power","0\n"); char s[8]; snprintf(s,sizeof s,"%d\n",g_user_bri); bl_w("brightness",s); }
static void bl_off(void){ bl_w("bl_power","1\n"); }
static void ss_update(void){
    if(!ss_time) return;
    time_t n=time(NULL); struct tm tmv; localtime_r(&n,&tmv);
    char h[8]; snprintf(h,sizeof h,"%02d:%02d",tmv.tm_hour,tmv.tm_min); lv_label_set_text(ss_time,h);
    static const char*dw[]={"Dom","Lun","Mar","Mie","Jue","Vie","Sab"},*mo[]={"Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"};
    char d[48]; snprintf(d,sizeof d,"%s %d %s",dw[tmv.tm_wday%7],tmv.tm_mday,mo[tmv.tm_mon%12]); lv_label_set_text(ss_date,d);
}
static void build_saver(void){
    if(g_scr_saver) return;
    g_scr_saver=lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_scr_saver,lv_color_hex(0x000000),0);
    lv_obj_clear_flag(g_scr_saver,LV_OBJ_FLAG_SCROLLABLE);
    ss_time=lv_label_create(g_scr_saver);
    lv_obj_set_style_text_color(ss_time,lv_color_hex(0xffffff),0);
    lv_obj_set_style_text_font(ss_time,&lv_font_montserrat_48,0);
    lv_obj_align(ss_time,LV_ALIGN_CENTER,0,-18);
    ss_date=lv_label_create(g_scr_saver);
    lv_obj_set_style_text_color(ss_date,lv_color_hex(0x7a8096),0);
    lv_obj_set_style_text_font(ss_date,&lv_font_montserrat_20,0);
    lv_obj_align(ss_date,LV_ALIGN_CENTER,0,40);
}
static void manage_idle(void){
    uint32_t idle=lv_disp_get_inactive_time(NULL);
    if(idle < IDLE_SAVER_MS){
        if(g_idle_state!=0){
            bl_on();
            if(g_scr_saver && lv_scr_act()==g_scr_saver) lv_scr_load(scr_home);
            g_idle_state=0;
        }
    } else if(idle < IDLE_OFF_MS){
        if(g_idle_state==0 && lv_scr_act()==scr_home){
            build_saver(); ss_update(); lv_scr_load(g_scr_saver); g_idle_state=1;
        }
        if(g_idle_state==1) ss_update();
    } else {
        if(g_idle_state!=2){ bl_off(); g_idle_state=2; }
    }
}

/* ------------------------------- botones --------------------------------- */
static int prev_btn=0;
static void poll_buttons(void){
    int bm=read_buttons();
    int newp=bm & ~prev_btn; prev_btn=bm;
    for(int k=0;k<3;k++) if(newp&(1<<k)){
        /* k-ésimo item de relé */
        int seen=0; Item*it=NULL;
        for(int i=0;i<n_items;i++) if(items[i].kind==1){ if(seen==k){ it=&items[i]; break; } seen++; }
        if(it){ int ns=(relay_get(it->relay)==1)?0:1; relay_set(it->relay,ns); it->state=ns; tile_set_visual(it,ns); }
    }
}

int main(int argc,char**argv){
    const char*cfg = (argc>1)? argv[1] : "/tuya/data/custom/tpp06_ui.conf";
    signal(SIGINT,on_sig); signal(SIGTERM,on_sig); signal(SIGPIPE,SIG_IGN);
    mount("none","/sys/kernel/debug","debugfs",0,NULL);
    load_config(cfg);
    fprintf(stderr,"config: %d items, ha_host=%s\n", n_items, g_host);

    if (fb_init()<0) return 1;
    tpfd = open("/dev/input/event1", O_RDONLY|O_NONBLOCK);

    lv_init();
    manrope(28); /* Fase1: valida pipeline Manrope(tiny_ttf). Se usará en el home. */
    static lv_disp_draw_buf_t ddb;
    lv_color_t *lvbuf = malloc(sizeof(lv_color_t)*SW*SH);
    lv_disp_draw_buf_init(&ddb, lvbuf, NULL, SW*SH);
    static lv_disp_drv_t ddrv; lv_disp_drv_init(&ddrv);
    ddrv.draw_buf=&ddb; ddrv.flush_cb=disp_flush;
    ddrv.hor_res=SW; ddrv.ver_res=SH; ddrv.full_refresh=1;
    lv_disp_drv_register(&ddrv);
    static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv);
    idrv.type=LV_INDEV_TYPE_POINTER; idrv.read_cb=touch_read;
    lv_indev_drv_register(&idrv);

    build_home();
    lv_timer_create(clock_update, 1000, NULL);   /* una sola vez */

    memset(&ha,0,sizeof ha); ha.fd=-1;
    long ha_next_try=0;
    int ha_mode = (g_host[0] && g_token[0]);
    if(ha_mode){
        for(int i=0;i<n_items;i++) if(items[i].kind==0) ha_watch(&ha, items[i].entity);
        if(ha_connect(&ha, g_host, g_port, g_token)==0) refresh_all();
        else ha_next_try=time(NULL)+5;
    }
    prev_btn=read_buttons();
    refresh_all();
    fprintf(stderr,"lvgl home listo\n");

    while (g_run){
        lv_timer_handler();
        long now=time(NULL);
        /* HA */
        if(ha.fd>=0){
            int pr=ha_poll(&ha);
            if(pr<0){ ha_close(&ha); ha_next_try=now+3; }
            else if(pr>0){ refresh_all(); }
        }
        if(ha_mode && ha.fd<0 && now>=ha_next_try){
            if(ha_connect(&ha,g_host,g_port,g_token)==0) refresh_all();
            else ha_next_try=now+5;
        }
        poll_buttons();
        manage_idle();
        usleep(5000);
        lv_tick_inc(5);
    }
    ha_close(&ha);
    return 0;
}

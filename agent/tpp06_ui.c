/* tpp06_ui — motor de interfaz en framebuffer para el TPP06 (ARMv5).
 *
 * Fase 3b.2: dibuja en /dev/fb0 (320x480, BGR565), lee el táctil goodix
 * (/dev/input/event1) y conmuta entidades REALES de Home Assistant vía su
 * API WebSocket (ha_ws). Los widgets reflejan el estado de HA en vivo
 * (subscribe_entities) y al tocarlos llaman al servicio toggle.
 *
 * Config (por defecto /tuya/data/custom/tpp06_ui.conf, o argv[1]):
 *   ha_host=192.168.18.77
 *   ha_port=8123
 *   ha_token=<token de larga duración>
 *   widget=light.salon_moessalon_switch_2|Mesa|light
 *   widget=switch.cocina|Cocina|switch
 *
 * Uso: tpp06_ui [config] [--seconds N]   (N>0: sale a los N s; 0: bucle infinito)
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
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/mount.h>
#include <linux/fb.h>
#include <linux/input.h>
#include "font8x16.h"
#include "ha_ws.h"

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, uint32_t)
#endif

/* ------------------------------ framebuffer ------------------------------ */
/* El panel TPP06 (ak-lcd) es 320x480, 16bpp BGR565, stride 640 (307200 B/frame).
   Confirmado leyendo el framebuffer vivo de la app Tuya (el logo sale perfecto a
   320x480/16bpp/stride640) y por el proyecto de la comunidad. smem_len=614400
   (doble buffer). El tipo de "color" es uint32_t pero solo usa 16 bits (BGR565).
   OJO: para que el write() al framebuffer se vea, la app Tuya debe estar MATADA
   (congelada retiene el LCD). */
static int fb_fd = -1, tp_fd = -1;
static uint16_t *back = NULL;
static int SW, SH, STRIDE, FBLEN;
static size_t SMEM = 0;

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b){
    /* BGR565 en 16 bits: rojo 0-4, verde 5-10, azul 11-15 */
    return ((uint32_t)(b>>3)<<11) | ((uint32_t)(g>>2)<<5) | (uint32_t)(r>>3);
}

static int fb_open(void){
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0){ perror("open fb0"); return -1; }
    struct fb_var_screeninfo v; struct fb_fix_screeninfo f;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &v) < 0){ perror("VSCREENINFO"); return -1; }
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &f) < 0){ perror("FSCREENINFO"); return -1; }
    SW = v.xres; SH = v.yres;
    STRIDE = f.line_length ? f.line_length : SW*2;   /* 640 */
    FBLEN  = STRIDE * SH;                              /* 307200 */
    SMEM   = f.smem_len ? f.smem_len : (size_t)FBLEN;
    fprintf(stderr,"fb %dx%d bpp=%d stride=%d FBLEN=%d smem=%zu\n",
            SW,SH,v.bits_per_pixel,STRIDE,FBLEN,SMEM);
    back = malloc(FBLEN);
    if (!back){ fprintf(stderr,"sin memoria back\n"); return -1; }
    /* despertar panel: unblank + backlight encendido */
    ioctl(fb_fd, FBIOBLANK, 0 /*FB_BLANK_UNBLANK*/);
    { int b=open("/sys/class/backlight/backlight/bl_power",O_WRONLY);
      if(b>=0){ if(write(b,"0\n",2)<0){} close(b); }
      b=open("/sys/class/backlight/backlight/brightness",O_WRONLY);
      if(b>=0){ if(write(b,"200\n",4)<0){} close(b); } }
    return 0;
}

static void fb_present(void){
    /* write() secuencial del frame (como dd). Si hay doble buffer en smem,
       escribimos la misma imagen en ambos para que se vea sea cual sea el activo. */
    int nbuf = (SMEM >= 2*(size_t)FBLEN) ? 2 : 1;
    for (int bi=0; bi<nbuf; bi++){
        if (lseek(fb_fd, (off_t)bi*FBLEN, SEEK_SET) == (off_t)-1) break;
        size_t off = 0;
        while (off < (size_t)FBLEN){
            ssize_t w = write(fb_fd, (const uint8_t*)back + off, FBLEN - off);
            if (w <= 0){ if (errno==EINTR) continue; break; }
            off += (size_t)w;
        }
    }
}

/* ------------------------------ primitivas (16bpp) ----------------------- */
static inline void px(int x,int y,uint32_t c){
    if ((unsigned)x<(unsigned)SW && (unsigned)y<(unsigned)SH)
        *(uint16_t*)((uint8_t*)back + y*STRIDE + x*2) = (uint16_t)c;
}
static void fill(uint32_t c){
    for (int y=0;y<SH;y++){ uint16_t *row=(uint16_t*)((uint8_t*)back+y*STRIDE);
        for (int x=0;x<SW;x++) row[x]=(uint16_t)c; }
}
static void rect(int x,int y,int w,int h,uint32_t c){
    for (int j=0;j<h;j++) for (int i=0;i<w;i++) px(x+i,y+j,c);
}
static void rect_outline(int x,int y,int w,int h,int t,uint32_t c){
    rect(x,y,w,t,c); rect(x,y+h-t,w,t,c); rect(x,y,t,h,c); rect(x+w-t,y,t,h,c);
}
static void glyph(int x,int y,char ch,uint32_t c,int sc){
    if (ch<FONT_FIRST || ch>126) ch='?';
    const unsigned char *g = font8x16[(int)ch-FONT_FIRST];
    for (int row=0;row<FONT_H;row++)
        for (int col=0;col<FONT_W;col++)
            if (g[row]&(1<<(7-col)))
                rect(x+col*sc, y+row*sc, sc, sc, c);
}
static int text_w(const char*s,int sc){ return (int)strlen(s)*FONT_W*sc; }
static void text(int x,int y,const char*s,uint32_t c,int sc){
    for (; *s; s++){ glyph(x,y,*s,c,sc); x += FONT_W*sc; }
}
static void text_center(int cx,int y,const char*s,uint32_t c,int sc){
    text(cx - text_w(s,sc)/2, y, s, c, sc);
}
/* dibuja 's' ajustándolo al ancho 'maxw': baja de escala 2->1 y si no cabe trunca */
static void text_fit(int x,int y,int h,const char*s,uint32_t c,int maxw){
    int sc = 2;
    if (text_w(s,sc) > maxw) sc = 1;
    char buf[48]; strncpy(buf,s,sizeof buf-1); buf[sizeof buf-1]=0;
    if (text_w(buf,sc) > maxw){
        int cw = FONT_W*sc, maxc = maxw/cw;
        if (maxc < 1) maxc = 1;
        if (maxc < (int)strlen(buf)){
            if (maxc >= 2){ buf[maxc-1]=0; strcat(buf,"."); } else buf[maxc]=0;
        }
    }
    text(x, y + h/2 - FONT_H*sc/2, buf, c, sc);
}

/* --------------------------------- táctil -------------------------------- */
static int tp_open(void){
    tp_fd = open("/dev/input/event1", O_RDONLY|O_NONBLOCK);
    if (tp_fd < 0) perror("open touch");
    return tp_fd;
}
static int tp_x=0, tp_y=0, tp_down=0;
/* procesa todos los eventos pendientes; *pressed=1 si bajó un dedo, *released=1 si
   se soltó. Mantiene tp_x/tp_y/tp_down. dev 1 si hubo eventos. */
static int tp_process(int *pressed, int *released){
    struct input_event ev; *pressed=0; *released=0; int any=0;
    while (read(tp_fd,&ev,sizeof ev) == (int)sizeof ev){
        any=1;
        if (ev.type==EV_ABS){
            if (ev.code==ABS_MT_POSITION_X || ev.code==ABS_X) tp_x=ev.value;
            else if (ev.code==ABS_MT_POSITION_Y || ev.code==ABS_Y) tp_y=ev.value;
            else if (ev.code==ABS_MT_TRACKING_ID){ if(ev.value==-1){ *released=1; tp_down=0; } else { *pressed=1; tp_down=1; } }
        } else if (ev.type==EV_KEY && ev.code==BTN_TOUCH){
            if (ev.value){ *pressed=1; tp_down=1; } else { *released=1; tp_down=0; }
        }
    }
    return any;
}

/* --------------------------------- UI ------------------------------------ */
static volatile int g_run=1;
static void on_sig(int s){ (void)s; g_run=0; }

#define COL_BG      rgb(18,18,24)
#define COL_BAR     rgb(32,34,44)
#define COL_TXT     rgb(235,235,240)
#define COL_DIM     rgb(150,150,160)
#define COL_ON      rgb(40,170,90)
#define COL_OFF     rgb(70,72,84)
#define COL_ONTX    rgb(240,255,245)

#define CONTENT_TOP 52          /* bajo la barra de título */
#define WIDGET_H    84
#define WIDGET_GAP  12
#define FOOTER_H    28

static int g_scroll = 0;        /* desplazamiento vertical del contenido */
static int g_max_scroll = 0;

/* --------------------------------- relés --------------------------------- */
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
/* botones físicos key_f1/2/3 = gpio 62/63/64 (entrada, activo-bajo). No dan
   eventos input ni se pueden exportar (reclamados), pero su nivel se lee en
   /sys/kernel/debug/gpio. dev máscara: bit0=f1,bit1=f2,bit2=f3; 1 = pulsado. */
static int read_buttons(void){
    int fd=open("/sys/kernel/debug/gpio",O_RDONLY);
    if(fd<0) return 0;
    char buf[8192]; int n=read(fd,buf,sizeof buf-1); close(fd);
    if(n<=0) return 0; buf[n]=0;
    int m=0;
    for(int k=0;k<3;k++){
        char name[8]; snprintf(name,sizeof name,"key_f%d",k+1);
        char*p=strstr(buf,name); if(!p) continue;
        char*eol=strchr(p,'\n'); if(!eol) eol=buf+n;
        char c=*eol; *eol=0;
        if(strstr(p," lo")) m|=(1<<k);     /* nivel lo = pulsado */
        *eol=c;
    }
    return m;
}

typedef struct {
    int x,y,w,h;
    int state;              /* 0 off, 1 on, -1 desconocido/no disponible */
    int kind;               /* 0 = entidad HA, 1 = relé local */
    int relay;              /* índice KC_CTLn si kind==1 */
    char label[32];
    char entity[HA_ID_LEN];
    char domain[16];
} Widget;

static void draw_widget(const Widget*b){
    int yy = b->y - g_scroll;                       /* posición en pantalla */
    if (yy >= SH || yy + b->h <= 0) return;         /* fuera de vista: no dibujar */
    rect(b->x,yy,b->w,b->h,COL_OFF);
    rect_outline(b->x,yy,b->w,b->h,2, rgb(100,102,115));
    /* pastilla de estado a la derecha */
    int pw=96, ph=48, prx=b->x+b->w-pw-16, pry=yy+(b->h-ph)/2;
    uint32_t pc; const char*st; uint32_t stc; uint32_t oc;
    if (b->state==1){ pc=COL_ON; st="ON"; stc=COL_ONTX; oc=rgb(90,220,130); }
    else if (b->state==0){ pc=rgb(55,57,68); st="OFF"; stc=COL_DIM; oc=rgb(100,102,115); }
    else { pc=rgb(55,57,68); st="N/D"; stc=rgb(200,170,90); oc=rgb(150,120,60); }
    rect(prx,pry,pw,ph,pc);
    rect_outline(prx,pry,pw,ph,2, oc);
    text_center(prx+pw/2, pry+ph/2-FONT_H, st, stc, 2);
    /* etiqueta a la izquierda, ajustada para no solapar la pastilla */
    int lx = b->x+18, maxw = prx - 12 - lx;
    text_fit(lx, yy, b->h, b->label, COL_TXT, maxw);
}
/* hit-test en coordenadas de pantalla (añade el scroll) */
static int hit(const Widget*b,int x,int y){
    int ly = y + g_scroll;
    return x>=b->x && x<b->x+b->w && ly>=b->y && ly<b->y+b->h;
}

static void render(Widget*w, int n, long up, int ha_on){
    fill(COL_BG);
    /* widgets (con scroll); pueden desbordar, luego los enmascaramos */
    for (int i=0;i<n;i++) draw_widget(&w[i]);
    /* barra superior (enmascara desborde por arriba) */
    rect(0,0,SW,CONTENT_TOP,COL_BAR);
    text_center(SW/2, 18, "TPP06  HOME", COL_TXT, 2);
    uint32_t dot = ha_on ? rgb(40,190,100) : rgb(200,80,70);
    rect(SW-26, 18, 16, 16, dot);
    /* pie (enmascara desborde por abajo) */
    rect(0, SH-FOOTER_H, SW, FOOTER_H, COL_BG);
    char ft[64]; snprintf(ft,sizeof ft,"uptime %lds", up);
    text(12, SH-24, ft, COL_DIM, 1);
    const char*hs = ha_on ? "HA conectado" : "HA ...";
    text(SW-text_w(hs,1)-12, SH-24, hs, COL_DIM, 1);
    /* indicador de scroll */
    if (g_max_scroll > 0){
        int track = SH - FOOTER_H - CONTENT_TOP;
        int bar = track*track/(track+g_max_scroll); if (bar<24) bar=24;
        int by = CONTENT_TOP + (track-bar)*g_scroll/g_max_scroll;
        rect(SW-5, by, 3, bar, rgb(95,97,115));
    }
}

/* ------------------------------ config ----------------------------------- */
static char g_host[40] = "";
static int  g_port = 8123;
static char g_token[1200] = "";

static void trim(char*s){
    char*e = s + strlen(s);
    while (e>s && (e[-1]=='\n'||e[-1]=='\r'||e[-1]==' '||e[-1]=='\t')) *--e=0;
    while (*s==' '||*s=='\t') memmove(s,s+1,strlen(s));
}

/* carga config; rellena widgets; dev número de widgets */
static int load_config(const char*path, Widget*w, int maxw){
    FILE*f = fopen(path, "r");
    if (!f){ fprintf(stderr,"ui: sin config %s\n", path); return 0; }
    char line[1400]; int n = 0;
    while (fgets(line, sizeof line, f)){
        trim(line);
        if (line[0]=='#' || line[0]==0) continue;
        char*eq = strchr(line,'=');
        if (!eq) continue;
        *eq = 0; char*key=line; char*val=eq+1;
        if (!strcmp(key,"ha_host")) strncpy(g_host,val,sizeof g_host-1);
        else if (!strcmp(key,"ha_port")) g_port = atoi(val);
        else if (!strcmp(key,"ha_token")) strncpy(g_token,val,sizeof g_token-1);
        else if (!strcmp(key,"widget") && n < maxw){
            /* entity|Label|domain */
            char*e1 = strchr(val,'|');
            char ent[HA_ID_LEN], lab[32], dom[16]="light";
            if (e1){
                *e1=0; strncpy(ent,val,sizeof ent-1); ent[sizeof ent-1]=0;
                char*rest=e1+1; char*e2=strchr(rest,'|');
                if (e2){ *e2=0; strncpy(lab,rest,sizeof lab-1); lab[sizeof lab-1]=0;
                         strncpy(dom,e2+1,sizeof dom-1); dom[sizeof dom-1]=0; }
                else { strncpy(lab,rest,sizeof lab-1); lab[sizeof lab-1]=0; }
            } else {
                strncpy(ent,val,sizeof ent-1); ent[sizeof ent-1]=0;
                strncpy(lab,val,sizeof lab-1); lab[sizeof lab-1]=0;
            }
            memset(&w[n],0,sizeof w[n]);
            w[n].kind = 0;
            strncpy(w[n].entity,ent,HA_ID_LEN-1);
            strncpy(w[n].label,lab,sizeof w[n].label-1);
            strncpy(w[n].domain,dom,sizeof w[n].domain-1);
            w[n].state = -1;
            n++;
        }
        else if (!strcmp(key,"relay") && n < maxw){
            /* N|Label  -> relé local KC_CTLn */
            char*bar = strchr(val,'|');
            int idx = atoi(val);
            char lab[32]; snprintf(lab,sizeof lab,"Rele %d",idx);
            if (bar){ strncpy(lab,bar+1,sizeof lab-1); lab[sizeof lab-1]=0; }
            memset(&w[n],0,sizeof w[n]);
            w[n].kind = 1; w[n].relay = idx;
            strncpy(w[n].label,lab,sizeof w[n].label-1);
            w[n].state = -1;
            n++;
        }
    }
    fclose(f);
    return n;
}

/* coloca los widgets en vertical (y lógica; el scroll se aplica al dibujar) */
static void layout(Widget*w, int n){
    int y = CONTENT_TOP + WIDGET_GAP;
    for (int i=0;i<n;i++){ w[i].x=20; w[i].y=y; w[i].w=SW-40; w[i].h=WIDGET_H; y += WIDGET_H+WIDGET_GAP; }
    int content = (n>0) ? (y - CONTENT_TOP) : 0;
    int viewport = SH - FOOTER_H - CONTENT_TOP;
    g_max_scroll = (content > viewport) ? (content - viewport) : 0;
}

/* sincroniza el estado de los widgets (HA o relé); dev 1 si cambió alguno */
static int sync_states(ha_client*c, Widget*w, int n){
    int ch = 0;
    for (int i=0;i<n;i++){
        int s = -1;
        if (w[i].kind == 1){ s = relay_get(w[i].relay); }
        else { ha_entity*e = ha_get(c, w[i].entity);
               if (e && e->known) s = (!strcmp(e->state,"on")) ? 1 : 0; }
        if (w[i].state != s){ w[i].state = s; ch = 1; }
    }
    return ch;
}

int main(int argc,char**argv){
    int seconds=0; const char*cfg = "/tuya/data/custom/tpp06_ui.conf";
    for (int i=1;i<argc;i++){
        if (!strcmp(argv[i],"--seconds") && i+1<argc) seconds=atoi(argv[++i]);
        else if (argv[i][0] != '-') cfg = argv[i];
    }
    signal(SIGINT,on_sig); signal(SIGTERM,on_sig); signal(SIGPIPE,SIG_IGN);
    mount("none","/sys/kernel/debug","debugfs",0,NULL);   /* para leer los botones */

    Widget wid[16];
    int n = load_config(cfg, wid, 16);
    int ha_mode = (g_host[0] && g_token[0]);
    fprintf(stderr,"ui: config %s -> %d widgets, ha_host=%s puerto=%d ha=%d\n",
            cfg, n, g_host, g_port, ha_mode);

    if (fb_open()<0) return 1;
    tp_open();
    layout(wid, n);

    ha_client ha; memset(&ha,0,sizeof ha); ha.fd=-1;
    long ha_next_try = 0;
    if (ha_mode){
        for (int i=0;i<n;i++) if (wid[i].kind==0) ha_watch(&ha, wid[i].entity);
        if (ha_connect(&ha, g_host, g_port, g_token)==0) sync_states(&ha,wid,n);
        else ha_next_try = time(NULL)+5;
    }
    sync_states(&ha, wid, n);   /* estado inicial de relés */

    time_t start=time(NULL); long up0=0;
    { char ub[64]; int fd=open("/proc/uptime",O_RDONLY); if(fd>=0){int r=read(fd,ub,63);if(r>0){ub[r]=0;up0=atol(ub);}close(fd);} }

    int ha_on = (ha.fd>=0 && ha.ready);
    render(wid,n, up0, ha_on); fb_present();
    long last_paint = time(NULL);
    int last_y=0, moved_acc=0, have_last=0;   /* estado de arrastre/scroll */
    int prev_btn = read_buttons();            /* estado previo de botones físicos */

    while (g_run){
        fd_set rf; FD_ZERO(&rf);
        int mx = 0;
        if (tp_fd>=0){ FD_SET(tp_fd,&rf); if(tp_fd>mx) mx=tp_fd; }
        if (ha.fd>=0){ FD_SET(ha.fd,&rf); if(ha.fd>mx) mx=ha.fd; }
        struct timeval tv={0,100000};
        int r = select(mx+1,&rf,NULL,NULL,&tv);
        int dirty=0;
        long now = time(NULL);

        /* táctil: arrastre = scroll, toque sin mover = conmutar */
        if (r>0 && tp_fd>=0 && FD_ISSET(tp_fd,&rf)){
            int pressed, released;
            if (tp_process(&pressed, &released)){
                if (pressed){ have_last=0; moved_acc=0; }
                if (tp_down){
                    if (!have_last){ have_last=1; last_y=tp_y; }
                    else {
                        int dy = tp_y - last_y;
                        if (dy){
                            moved_acc += (dy<0?-dy:dy);
                            g_scroll -= dy;
                            if (g_scroll<0) g_scroll=0;
                            if (g_scroll>g_max_scroll) g_scroll=g_max_scroll;
                            last_y = tp_y;
                            dirty=1;
                        }
                    }
                }
                if (released){
                    if (moved_acc < 10){            /* toque: conmutar el widget */
                        for (int i=0;i<n;i++) if (hit(&wid[i],tp_x,tp_y)){
                            if (wid[i].kind==1){       /* relé local */
                                int ns = (wid[i].state==1)?0:1;
                                relay_set(wid[i].relay, ns);
                                wid[i].state = ns;
                                dirty=1;
                            } else if (ha.fd>=0 && ha.ready){
                                wid[i].state = (wid[i].state==1)?0:1;
                                ha_call(&ha, wid[i].domain, "toggle", wid[i].entity);
                                dirty=1;
                            }
                            break;
                        }
                    }
                    have_last=0; moved_acc=0;
                }
            }
        }
        /* botones físicos: el botón fN conmuta el N-ésimo widget de relé (en
           orden de pantalla), para que botones y pantalla coincidan siempre */
        {
            int bm = read_buttons();
            int newpress = bm & ~prev_btn;       /* flancos hi->lo */
            prev_btn = bm;
            for (int k=0;k<3;k++) if (newpress & (1<<k)){
                int seen=0, wi=-1;
                for (int i=0;i<n;i++) if (wid[i].kind==1){ if(seen==k){ wi=i; break; } seen++; }
                if (wi>=0){
                    int relN = wid[wi].relay;
                    int ns = (relay_get(relN)==1) ? 0 : 1;
                    relay_set(relN, ns);
                    wid[wi].state = ns;
                    fprintf(stderr,"boton f%d -> widget %d (KC_CTL%d)=%d\n",k+1,wi,relN,ns);
                    dirty=1;
                }
            }
        }
        /* datos de HA */
        if (ha.fd>=0 && FD_ISSET(ha.fd,&rf)){
            int pr = ha_poll(&ha);
            if (pr < 0){ fprintf(stderr,"ha: caída, reconecto\n"); ha_close(&ha); ha_next_try = now+3; dirty=1; }
            else if (pr > 0){ if (sync_states(&ha,wid,n)) dirty=1; }
        }
        /* reconexión */
        if (ha_mode && ha.fd<0 && now>=ha_next_try){
            if (ha_connect(&ha, g_host, g_port, g_token)==0){ sync_states(&ha,wid,n); dirty=1; }
            else ha_next_try = now+5;
        }

        int on_now = (ha.fd>=0 && ha.ready);
        if (on_now != ha_on){ ha_on = on_now; dirty=1; }
        /* repintado periódico de seguridad (1 Hz): asegura el volcado al LCD */
        if (now - last_paint >= 1) dirty=1;
        if (dirty){ render(wid,n, up0 + (time(NULL)-start), ha_on); fb_present(); last_paint = time(NULL); }
        if (seconds>0 && time(NULL)-start >= seconds) break;
    }
    ha_close(&ha);
    fprintf(stderr,"fin ui\n");
    return 0;
}

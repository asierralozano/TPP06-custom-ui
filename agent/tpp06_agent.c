/* tpp06_agent — agente MQTT para el panel Tuya TPP06 (ARMv5, static).
 *
 * Fase 3a: expone el panel a Home Assistant por MQTT (con auto-discovery) y
 * acepta comandos. Convive con la app Tuya (no toca el framebuffer; solo el
 * backlight por sysfs). Dirigido por un archivo de config (pensado para un
 * instalador plug-and-play: el device id se deriva de la MAC).
 *
 * Sin dependencias externas: implementa MQTT 3.1.1 (QoS 0) sobre TCP plano.
 *
 * Entidades publicadas a HA (device "TPP06 <id>"):
 *   - light  : pantalla (backlight on/off + brillo 0..255)
 *   - sensor : uptime, señal wifi, memoria libre, IP
 *   - button : reiniciar
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

/* ----------------------------- utilidades -------------------------------- */

static volatile int g_run = 1;
static void on_sig(int s){ (void)s; g_run = 0; }

static void logmsg(const char *fmt, ...){
    char ts[32]; time_t t = time(NULL); struct tm tm;
    localtime_r(&t, &tm); strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(stderr, "[%s] ", ts);
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fputc('\n', stderr); fflush(stderr);
}

/* lee un archivo entero (pequeño) a buf; devuelve longitud o -1 */
static int read_file(const char *path, char *buf, int cap){
    int fd = open(path, O_RDONLY); if (fd < 0) return -1;
    int n = read(fd, buf, cap - 1); close(fd);
    if (n < 0) return -1; buf[n] = 0; return n;
}
static int write_file(const char *path, const char *val){
    int fd = open(path, O_WRONLY); if (fd < 0) return -1;
    int n = write(fd, val, strlen(val)); close(fd);
    return n < 0 ? -1 : 0;
}
static void trim(char *s){
    int n = strlen(s);
    while (n > 0 && (s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' '||s[n-1]=='\t')) s[--n]=0;
}

/* ------------------------------ config ----------------------------------- */

typedef struct {
    char host[128], user[64], pass[64];
    int  port;
    char base[64];       /* prefijo de topics propio, p.ej. "tpp06" */
    char disc[64];       /* discovery_prefix de HA, p.ej. "homeassistant" */
    char id[64];         /* device id único */
    char name[64];       /* nombre legible */
    int  telemetry_s;    /* intervalo de telemetría */
    char bl_path[128];   /* /sys/class/backlight/backlight */
    int  bl_max;
} Config;

static void mac_id(char *out, int cap){
    char buf[64]; int n = read_file("/sys/class/net/wlan0/address", buf, sizeof buf);
    if (n <= 0){ snprintf(out, cap, "unknown"); return; }
    trim(buf);
    /* aa:bb:cc:dd:ee:ff -> tpp06_ddeeff (últimos 3 octetos, sin dos puntos) */
    char hex[16]; int j=0;
    for (int i=0; buf[i] && j<12; i++) if (buf[i] != ':') hex[j++]=buf[i];
    hex[j]=0;
    if (j >= 6) snprintf(out, cap, "tpp06_%s", hex + (j-6));
    else        snprintf(out, cap, "tpp06_%s", hex);
}

static void config_defaults(Config *c){
    memset(c, 0, sizeof *c);
    c->port = 1883;
    strcpy(c->base, "tpp06");
    strcpy(c->disc, "homeassistant");
    c->telemetry_s = 30;
    strcpy(c->bl_path, "/sys/class/backlight/backlight");
    c->bl_max = 255;
    mac_id(c->id, sizeof c->id);
    snprintf(c->name, sizeof c->name, "TPP06 %s", c->id + 6); /* sin "tpp06_" */
}

/* parsea key=value; # comentario. Devuelve 0 si ok */
static int config_load(Config *c, const char *path){
    char buf[4096]; int n = read_file(path, buf, sizeof buf);
    if (n < 0) return -1;
    char *line = buf, *save;
    for (line = strtok_r(buf, "\n", &save); line; line = strtok_r(NULL, "\n", &save)){
        while (*line==' '||*line=='\t') line++;
        if (*line=='#'||*line==0) continue;
        char *eq = strchr(line, '='); if (!eq) continue;
        *eq = 0; char *k = line, *v = eq+1; trim(k); trim(v);
        if      (!strcmp(k,"broker_host")) snprintf(c->host,sizeof c->host,"%s",v);
        else if (!strcmp(k,"broker_port")) c->port = atoi(v);
        else if (!strcmp(k,"broker_user")) snprintf(c->user,sizeof c->user,"%s",v);
        else if (!strcmp(k,"broker_pass")) snprintf(c->pass,sizeof c->pass,"%s",v);
        else if (!strcmp(k,"base_topic"))  snprintf(c->base,sizeof c->base,"%s",v);
        else if (!strcmp(k,"discovery_prefix")) snprintf(c->disc,sizeof c->disc,"%s",v);
        else if (!strcmp(k,"device_id"))   snprintf(c->id,sizeof c->id,"%s",v);
        else if (!strcmp(k,"device_name")) snprintf(c->name,sizeof c->name,"%s",v);
        else if (!strcmp(k,"telemetry_s")) c->telemetry_s = atoi(v);
        else if (!strcmp(k,"backlight_path")) snprintf(c->bl_path,sizeof c->bl_path,"%s",v);
    }
    return 0;
}

/* --------------------------- MQTT 3.1.1 (QoS0) --------------------------- */

typedef struct { int fd; unsigned char buf[2048]; } Mqtt;

/* Conexión TCP a una IP numérica (sin getaddrinfo: evita la dependencia de
 * NSS en binarios glibc estáticos). El broker se configura por IP. */
static int tcp_connect(const char *host, int port){
    struct sockaddr_in sa; memset(&sa,0,sizeof sa);
    sa.sin_family = AF_INET; sa.sin_port = htons(port);
    if (inet_aton(host, &sa.sin_addr) == 0){
        logmsg("broker_host debe ser una IP numérica (recibido '%s')", host);
        return -1;
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0); if (fd < 0) return -1;
    if (connect(fd, (struct sockaddr*)&sa, sizeof sa) != 0){ close(fd); return -1; }
    int one=1; setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&one,sizeof one);
    return fd;
}

/* escribe 'len' bytes completos */
static int writen(int fd, const void *p, int len){
    const char *b = p; int off = 0;
    while (off < len){ int n = write(fd, b+off, len-off); if (n<=0) return -1; off+=n; }
    return 0;
}
/* codifica remaining length (varint MQTT) en out, devuelve nº de bytes */
static int enc_rl(unsigned char *out, int len){
    int i=0; do { unsigned char d = len % 128; len /= 128; if (len) d |= 0x80; out[i++]=d; } while (len);
    return i;
}
/* añade string MQTT (2 bytes len + bytes) a p, devuelve avance */
static int put_str(unsigned char *p, const char *s){
    int l = strlen(s); p[0]=l>>8; p[1]=l&0xff; memcpy(p+2,s,l); return l+2;
}

static int mqtt_connect(Mqtt *m, const Config *c){
    unsigned char vh[512]; int i=0;
    i += put_str(vh+i, "MQTT");          /* protocol name */
    vh[i++] = 4;                          /* protocol level 3.1.1 */
    int flags = 0x02;                     /* clean session */
    if (c->user[0]) flags |= 0x80;
    if (c->pass[0]) flags |= 0x40;
    /* will (LWT) para availability */
    char will_topic[128]; snprintf(will_topic,sizeof will_topic,"%s/%s/status",c->base,c->id);
    flags |= 0x04;                        /* will flag */
    flags |= 0x20;                        /* will retain */
    vh[i++] = flags;
    vh[i++] = 0; vh[i++] = 40;            /* keepalive 40s */
    i += put_str(vh+i, c->id);            /* client id */
    i += put_str(vh+i, will_topic);       /* will topic */
    i += put_str(vh+i, "offline");        /* will message */
    if (c->user[0]) i += put_str(vh+i, c->user);
    if (c->pass[0]) i += put_str(vh+i, c->pass);

    unsigned char hdr[8]; hdr[0]=0x10; int rl = enc_rl(hdr+1, i);
    if (writen(m->fd, hdr, 1+rl) || writen(m->fd, vh, i)) return -1;

    /* esperar CONNACK */
    unsigned char r[4]; int off=0;
    while (off<4){ int n=read(m->fd,r+off,4-off); if (n<=0) return -1; off+=n; }
    if (r[0] != 0x20 || r[3] != 0x00) { logmsg("CONNACK rc=%d", r[3]); return -1; }
    return 0;
}

static int mqtt_publish(Mqtt *m, const char *topic, const char *payload, int retain){
    int tl = strlen(topic), pl = payload?strlen(payload):0;
    int rem = 2 + tl + pl;
    unsigned char hdr[8]; hdr[0] = 0x30 | (retain?1:0); int rl = enc_rl(hdr+1, rem);
    unsigned char ts[2] = { tl>>8, tl&0xff };
    if (writen(m->fd,hdr,1+rl)||writen(m->fd,ts,2)||writen(m->fd,topic,tl)) return -1;
    if (pl && writen(m->fd,payload,pl)) return -1;
    return 0;
}

static int mqtt_subscribe(Mqtt *m, const char *topic){
    int tl = strlen(topic); int rem = 2 + 2 + tl + 1; /* pktid + topic + qos */
    unsigned char hdr[8]; hdr[0]=0x82; int rl=enc_rl(hdr+1,rem);
    unsigned char vh[256]; int i=0; vh[i++]=0; vh[i++]=1; /* packet id 1 */
    i += put_str(vh+i, topic); vh[i++]=0; /* qos 0 */
    return (writen(m->fd,hdr,1+rl)||writen(m->fd,vh,i)) ? -1 : 0;
}

static int mqtt_ping(Mqtt *m){ unsigned char p[2]={0xC0,0x00}; return writen(m->fd,p,2); }

/* --------------------------- lógica del panel ---------------------------- */

static Config C;

static int bl_brightness(void){
    char p[160], b[32]; snprintf(p,sizeof p,"%s/brightness",C.bl_path);
    if (read_file(p,b,sizeof b)<=0) return -1; return atoi(b);
}
static int bl_power_on(void){
    char p[160], b[32]; snprintf(p,sizeof p,"%s/bl_power",C.bl_path);
    if (read_file(p,b,sizeof b)<=0) return 1; return atoi(b)==0; /* 0 = encendida */
}
static void bl_set_power(int on){
    char p[160]; snprintf(p,sizeof p,"%s/bl_power",C.bl_path);
    write_file(p, on? "0":"4");                       /* FB_BLANK: 0=on, 4=off */
    write_file("/sys/class/graphics/fb0/blank", on? "0":"1");
}
static void bl_set_brightness(int v){
    if (v<0) v=0; if (v>C.bl_max) v=C.bl_max;
    char p[160], s[16]; snprintf(p,sizeof p,"%s/brightness",C.bl_path);
    snprintf(s,sizeof s,"%d",v); write_file(p,s);
}
static long uptime_s(void){ char b[64]; if(read_file("/proc/uptime",b,sizeof b)<=0)return -1; return atol(b); }
static int wifi_signal(void){
    char b[512]; if(read_file("/proc/net/wireless",b,sizeof b)<=0) return 0;
    char *l = strstr(b,"wlan0"); if(!l) return 0;
    int q=0; sscanf(l,"%*[^:]: %*d %d", &q); return q; /* link quality */
}
static long mem_free_kb(void){
    char b[2048]; if(read_file("/proc/meminfo",b,sizeof b)<=0) return 0;
    char *l = strstr(b,"MemAvailable:"); if(!l){ l=strstr(b,"MemFree:"); if(!l) return 0; }
    long v=0; sscanf(l,"%*[^0-9]%ld",&v); return v;
}
static void wlan_ip(char *out,int cap){
    snprintf(out,cap,"?");
    int s = socket(AF_INET,SOCK_DGRAM,0); if (s<0) return;
    struct ifreq ifr; memset(&ifr,0,sizeof ifr);
    strncpy(ifr.ifr_name,"wlan0",IFNAMSIZ-1);
    if (ioctl(s,SIOCGIFADDR,&ifr)==0){
        struct sockaddr_in *sin = (struct sockaddr_in*)&ifr.ifr_addr;
        const char *ip = inet_ntoa(sin->sin_addr);
        if (ip) snprintf(out,cap,"%s",ip);
    }
    close(s);
}

/* ------------------------------ topics ----------------------------------- */

static char T_avail[160], T_scr_state[160], T_scr_set[160],
            T_bri_state[160], T_bri_set[160], T_reboot_set[160],
            T_up[160], T_wifi[160], T_mem[160], T_ip[160];

static void build_topics(void){
    #define MK(dst,suf) snprintf(dst,sizeof dst,"%s/%s/" suf,C.base,C.id)
    MK(T_avail,"status");
    MK(T_scr_state,"screen/state");  MK(T_scr_set,"screen/set");
    MK(T_bri_state,"screen/brightness"); MK(T_bri_set,"screen/brightness/set");
    MK(T_reboot_set,"reboot/set");
    MK(T_up,"sensor/uptime"); MK(T_wifi,"sensor/wifi");
    MK(T_mem,"sensor/mem"); MK(T_ip,"sensor/ip");
    #undef MK
}

/* bloque "device" común para agrupar entidades en HA */
static int dev_json(char *o,int cap){
    return snprintf(o,cap,
      "\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\",\"mf\":\"Tuya/MOES\",\"mdl\":\"TPP06\"},"
      "\"avty_t\":\"%s\",\"pl_avail\":\"online\",\"pl_not_avail\":\"offline\"",
      C.id, C.name, T_avail);
}

static void publish_discovery(Mqtt *m){
    char cfg[1024], dev[384]; dev_json(dev,sizeof dev);
    /* light: pantalla */
    char t[200]; snprintf(t,sizeof t,"%s/light/%s/screen/config",C.disc,C.id);
    snprintf(cfg,sizeof cfg,
      "{\"name\":\"Pantalla\",\"uniq_id\":\"%s_screen\","
      "\"cmd_t\":\"%s\",\"stat_t\":\"%s\","
      "\"bri_cmd_t\":\"%s\",\"bri_stat_t\":\"%s\",\"bri_scl\":%d,"
      "\"pl_on\":\"ON\",\"pl_off\":\"OFF\",%s}",
      C.id,T_scr_set,T_scr_state,T_bri_set,T_bri_state,C.bl_max,dev);
    mqtt_publish(m,t,cfg,1);
    /* button: reiniciar */
    snprintf(t,sizeof t,"%s/button/%s/reboot/config",C.disc,C.id);
    snprintf(cfg,sizeof cfg,
      "{\"name\":\"Reiniciar\",\"uniq_id\":\"%s_reboot\",\"cmd_t\":\"%s\","
      "\"pl_prs\":\"PRESS\",\"dev_cla\":\"restart\",%s}",C.id,T_reboot_set,dev);
    mqtt_publish(m,t,cfg,1);
    /* sensores */
    #define SENSOR(slug,nm,unit,extra,topic) \
      snprintf(t,sizeof t,"%s/sensor/%s/" slug "/config",C.disc,C.id); \
      snprintf(cfg,sizeof cfg,"{\"name\":\"" nm "\",\"uniq_id\":\"%s_" slug "\"," \
        "\"stat_t\":\"%s\"" unit extra ",%s}",C.id,topic,dev); \
      mqtt_publish(m,t,cfg,1)
    SENSOR("uptime","Uptime",",\"unit_of_meas\":\"s\"",",\"ic\":\"mdi:clock-outline\"",T_up);
    SENSOR("wifi","Señal WiFi","",",\"ic\":\"mdi:wifi\"",T_wifi);
    SENSOR("mem","Memoria libre",",\"unit_of_meas\":\"kB\"",",\"ic\":\"mdi:memory\"",T_mem);
    SENSOR("ip","IP","",",\"ic\":\"mdi:ip-network\"",T_ip);
    #undef SENSOR
}

static void publish_state(Mqtt *m){
    char v[64];
    mqtt_publish(m,T_scr_state, bl_power_on()?"ON":"OFF",1);
    int b = bl_brightness(); if(b>=0){ snprintf(v,sizeof v,"%d",b); mqtt_publish(m,T_bri_state,v,1); }
    snprintf(v,sizeof v,"%ld",uptime_s()); mqtt_publish(m,T_up,v,0);
    snprintf(v,sizeof v,"%d",wifi_signal()); mqtt_publish(m,T_wifi,v,0);
    snprintf(v,sizeof v,"%ld",mem_free_kb()); mqtt_publish(m,T_mem,v,0);
    wlan_ip(v,sizeof v); mqtt_publish(m,T_ip,v,0);
}

static void subscribe_all(Mqtt *m){
    mqtt_subscribe(m,T_scr_set); mqtt_subscribe(m,T_bri_set); mqtt_subscribe(m,T_reboot_set);
}

/* procesa un PUBLISH entrante (comando) */
static void handle_cmd(Mqtt *m, const char *topic, const char *payload){
    logmsg("cmd %s = %s", topic, payload);
    if (!strcmp(topic,T_scr_set)){
        bl_set_power(!strcmp(payload,"ON")); publish_state(m);
    } else if (!strcmp(topic,T_bri_set)){
        bl_set_brightness(atoi(payload));
        if (atoi(payload)>0) bl_set_power(1);
        publish_state(m);
    } else if (!strcmp(topic,T_reboot_set)){
        logmsg("reboot solicitado"); sync(); system("reboot");
    }
}

/* lee y procesa paquetes entrantes disponibles (no bloqueante por select) */
static int mqtt_pump(Mqtt *m){
    unsigned char b[2048]; int n = read(m->fd,b,sizeof b);
    if (n <= 0) return -1;
    int i = 0;
    while (i < n){
        int type = b[i] & 0xf0; i++;
        int rl=0, mult=1, rb;
        do { if(i>=n) return 0; rb=b[i++]; rl += (rb&0x7f)*mult; mult*=128; } while(rb&0x80);
        if (i+rl > n) return 0; /* paquete incompleto: simplificación QoS0 */
        if (type == 0x30){ /* PUBLISH */
            int tl = (b[i]<<8)|b[i+1];
            char topic[200], payload[256];
            int cl = tl<199?tl:199; memcpy(topic,b+i+2,cl); topic[cl]=0;
            int po = i+2+tl, plen = rl-2-tl;
            if (plen<0) plen=0; int pc = plen<255?plen:255;
            memcpy(payload,b+po,pc); payload[pc]=0;
            handle_cmd(m, topic, payload);
        }
        /* PINGRESP (0xD0), SUBACK (0x90): ignorar */
        i += rl;
    }
    return 0;
}

int main(int argc, char **argv){
    const char *cfgpath = argc>1 ? argv[1] : "/tuya/data/custom/tpp06.conf";
    signal(SIGINT,on_sig); signal(SIGTERM,on_sig); signal(SIGPIPE,SIG_IGN);
    config_defaults(&C);
    if (config_load(&C,cfgpath)==0) logmsg("config: %s",cfgpath);
    else logmsg("sin config (%s); usando valores por defecto",cfgpath);
    build_topics();
    logmsg("id=%s name=%s broker=%s:%d",C.id,C.name,C.host,C.port);
    if (!C.host[0]){ logmsg("falta broker_host en la config"); return 1; }

    while (g_run){
        Mqtt m; m.fd = tcp_connect(C.host,C.port);
        if (m.fd < 0){ logmsg("no conecta a %s:%d, reintento en 5s",C.host,C.port); sleep(5); continue; }
        if (mqtt_connect(&m,&C)!=0){ logmsg("CONNECT falló"); close(m.fd); sleep(5); continue; }
        logmsg("conectado al broker");
        mqtt_publish(&m,T_avail,"online",1);
        publish_discovery(&m);
        subscribe_all(&m);
        publish_state(&m);

        time_t last_tele = 0, last_ping = time(NULL);
        while (g_run){
            fd_set rf; FD_ZERO(&rf); FD_SET(m.fd,&rf);
            struct timeval tv = {1,0};
            int r = select(m.fd+1,&rf,NULL,NULL,&tv);
            if (r<0){ if(errno==EINTR) continue; break; }
            if (r>0 && FD_ISSET(m.fd,&rf)){ if (mqtt_pump(&m)<0){ logmsg("broker cerró"); break; } }
            time_t now = time(NULL);
            if (now - last_tele >= C.telemetry_s){ publish_state(&m); last_tele=now; }
            if (now - last_ping >= 25){ if(mqtt_ping(&m)<0) break; last_ping=now; }
        }
        mqtt_publish(&m,T_avail,"offline",1);
        close(m.fd);
        if (g_run) sleep(3);
    }
    logmsg("saliendo");
    return 0;
}

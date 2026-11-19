/* ha_ws — cliente Home Assistant sobre WebSocket para el TPP06 (ARMv5, static).
 *
 * Minimalista y sin dependencias: handshake WS (RFC6455), masking de frames
 * cliente->servidor, y el protocolo de HA sobre `subscribe_entities` (estado
 * inicial + diffs, solo las entidades vigiladas => payloads pequeños).
 *
 * Uso típico:
 *   ha_client c; memset(&c,0,sizeof c); c.fd=-1;
 *   ha_watch(&c,"light.salon_moessalon_switch_2");
 *   ha_connect(&c,"192.168.18.77",8123,token);   // deja fd NO bloqueante
 *   // en el bucle: cuando c.fd sea legible (o periódicamente):
 *   int r = ha_poll(&c);   // 1=cambió algo, 0=nada, -1=cayó la conexión
 *   ha_call(&c,"light","toggle","light.salon_moessalon_switch_2");
 */
#ifndef HA_WS_H
#define HA_WS_H

#define HA_MAX_ENT 24
#define HA_ID_LEN  80

typedef struct {
    char id[HA_ID_LEN];   /* entity_id */
    char state[24];       /* "on","off","unavailable",... */
    int  brightness;      /* 0-255, -1 si n/a */
    int  position;        /* covers: 0-100, -1 si n/a */
    int  known;           /* 1 si hemos recibido estado alguna vez */
} ha_entity;

typedef struct {
    int  fd;              /* socket; -1 si desconectado */
    int  ready;           /* 1 tras auth_ok + subscribe enviado */
    int  next_id;         /* contador de id de mensajes HA */
    char ip[40];
    int  port;
    const char *token;    /* debe seguir vivo mientras se use el cliente */
    ha_entity ent[HA_MAX_ENT];
    int  n;
    unsigned char rx[16384];
    int  rxlen;
    long last_rx;         /* time() del último dato recibido */
} ha_client;

/* registra una entidad a vigilar (antes del primer connect). dev 0 ok, -1 lleno */
int  ha_watch(ha_client *c, const char *entity_id);
/* conecta + handshake + auth + subscribe_entities; deja fd no bloqueante.
   mantiene la lista de entidades (sirve para reconectar). dev 0 ok, -1 error */
int  ha_connect(ha_client *c, const char *ip, int port, const char *token);
/* drena el socket (no bloqueante) y aplica cambios. dev 1=cambió, 0=no, -1=caída */
int  ha_poll(ha_client *c);
/* llama a un servicio sobre una entidad. dev 0 ok, -1 error */
int  ha_call(ha_client *c, const char *domain, const char *service, const char *entity_id);
/* enciende una luz con brillo (0-100%). dev 0 ok, -1 error */
int  ha_call_bri(ha_client *c, const char *entity_id, int pct);
/* fija la posición de un cover (0-100%). dev 0 ok, -1 error */
int  ha_call_cover_pos(ha_client *c, const char *entity_id, int pct);
/* busca entidad por id; NULL si no está */
ha_entity *ha_get(ha_client *c, const char *entity_id);
/* cierra el socket */
void ha_close(ha_client *c);

#endif

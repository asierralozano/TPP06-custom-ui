/* ha_ws — implementación. Ver ha_ws.h. */
#define _GNU_SOURCE
#include "ha_ws.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

/* ------------------------------ utilidades ------------------------------- */
static void rand_bytes(unsigned char *b, int n){
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0){ int r = read(fd, b, n); close(fd); if (r == n) return; }
    for (int i = 0; i < n; i++) b[i] = (unsigned char)(rand() ^ (i*31) ^ (int)time(0));
}

static void b64(const unsigned char *in, int n, char *out){
    static const char T[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int i, o = 0;
    for (i = 0; i + 2 < n; i += 3){
        int v = (in[i]<<16)|(in[i+1]<<8)|in[i+2];
        out[o++]=T[(v>>18)&63]; out[o++]=T[(v>>12)&63];
        out[o++]=T[(v>>6)&63];  out[o++]=T[v&63];
    }
    int rem = n - i;
    if (rem == 1){ int v = in[i]<<16;
        out[o++]=T[(v>>18)&63]; out[o++]=T[(v>>12)&63]; out[o++]='='; out[o++]='='; }
    else if (rem == 2){ int v = (in[i]<<16)|(in[i+1]<<8);
        out[o++]=T[(v>>18)&63]; out[o++]=T[(v>>12)&63]; out[o++]=T[(v>>6)&63]; out[o++]='='; }
    out[o] = 0;
}

static int write_all(int fd, const void *buf, int len){
    const char *p = buf; int left = len;
    while (left > 0){
        int w = send(fd, p, left, MSG_NOSIGNAL);
        if (w < 0){ if (errno == EINTR) continue; return -1; }
        p += w; left -= w;
    }
    return 0;
}

static int tcp_connect(const char *ip, int port){
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in sa; memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET; sa.sin_port = htons(port);
    if (!inet_aton(ip, &sa.sin_addr)){ close(fd); return -1; }
    if (connect(fd, (struct sockaddr*)&sa, sizeof sa) < 0){ close(fd); return -1; }
    return fd;
}

/* ------------------------------ frames WS -------------------------------- */
/* cliente->servidor DEBE enmascarar. opcode: 0x1 texto, 0x8 close, 0xA pong */
static int ws_send_frame(int fd, int opcode, const unsigned char *p, int len){
    unsigned char hdr[14]; int hl = 0;
    hdr[0] = 0x80 | opcode;               /* FIN + opcode */
    unsigned char mask[4]; rand_bytes(mask, 4);
    if (len < 126){ hdr[1] = 0x80 | len; hl = 2; }
    else if (len < 65536){ hdr[1] = 0x80|126; hdr[2]=(len>>8)&0xff; hdr[3]=len&0xff; hl = 4; }
    else { hdr[1] = 0x80|127; for (int i=0;i<8;i++) hdr[2+i]=(unsigned char)(((unsigned long long)len)>>(8*(7-i))); hl = 10; }
    memcpy(hdr+hl, mask, 4); hl += 4;
    if (write_all(fd, hdr, hl) < 0) return -1;
    unsigned char buf[1024]; int off = 0;
    while (off < len){
        int n = len - off; if (n > (int)sizeof buf) n = sizeof buf;
        for (int i = 0; i < n; i++) buf[i] = p[off+i] ^ mask[(off+i)&3];
        if (write_all(fd, buf, n) < 0) return -1;
        off += n;
    }
    return 0;
}
static int ws_send_text(int fd, const char *s, int len){
    return ws_send_frame(fd, 0x1, (const unsigned char*)s, len);
}

/* extrae UN frame completo del buffer rx. dev: 1 con payload en out (NUL-term),
   0 si aún no hay frame completo, -1 si un frame no cabe jamás en rx (fatal). */
static int extract_frame(ha_client *c, unsigned char *out, int outcap, int *plen, int *opret){
    if (c->rxlen < 2) return 0;
    unsigned char b1 = c->rx[1];
    int op = c->rx[0] & 0x0f;
    int ln = b1 & 0x7f;
    int idx = 2;
    unsigned long long L = ln;
    if (ln == 126){ if (c->rxlen < 4) return 0; L = ((unsigned)c->rx[2]<<8)|c->rx[3]; idx = 4; }
    else if (ln == 127){ if (c->rxlen < 10) return 0; L = 0; for (int i=0;i<8;i++) L=(L<<8)|c->rx[2+i]; idx = 10; }
    if (idx + L > sizeof c->rx) return -1;            /* no cabrá nunca -> fatal */
    if ((unsigned long long)c->rxlen < idx + L) return 0;  /* incompleto */
    int n = (int)L; if (n > outcap-1) n = outcap-1;   /* truncamos copia (no esperado) */
    memcpy(out, c->rx + idx, n); out[n] = 0; *plen = n; *opret = op;
    int consumed = idx + (int)L;
    memmove(c->rx, c->rx + consumed, c->rxlen - consumed);
    c->rxlen -= consumed;
    return 1;
}

/* lee (bloqueante, con SO_RCVTIMEO) hasta un frame de texto. dev len o -1 */
static int read_text_message(ha_client *c, char *out, int cap){
    for (;;){
        int plen, op;
        int e = extract_frame(c, (unsigned char*)out, cap, &plen, &op);
        if (e < 0) return -1;
        if (e == 1){
            if (op == 0x1 || op == 0x2) return plen;      /* texto/binario */
            if (op == 0x8) return -1;                     /* close */
            if (op == 0x9){ ws_send_frame(c->fd, 0xA, (unsigned char*)out, plen); continue; } /* ping->pong */
            continue;                                      /* pong/continuación */
        }
        if (c->rxlen >= (int)sizeof c->rx) return -1;     /* lleno sin frame */
        int r = recv(c->fd, c->rx + c->rxlen, sizeof c->rx - c->rxlen, 0);
        if (r <= 0) return -1;
        c->rxlen += r;
    }
}

/* ------------------------------ handshake -------------------------------- */
static int ws_handshake(int fd, const char *ip, int port){
    unsigned char k[16]; rand_bytes(k, 16);
    char key[32]; b64(k, 16, key);
    char req[512];
    int n = snprintf(req, sizeof req,
        "GET /api/websocket HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\n"
        "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",
        ip, port, key);
    if (write_all(fd, req, n) < 0) return -1;
    char buf[1024]; int len = 0;
    while (len < (int)sizeof buf - 1){
        int r = recv(fd, buf + len, 1, 0);
        if (r <= 0) return -1;
        len++;
        if (len >= 4 && buf[len-4]=='\r'&&buf[len-3]=='\n'&&buf[len-2]=='\r'&&buf[len-1]=='\n') break;
    }
    buf[len] = 0;
    if (!strstr(buf, " 101 ")){ fprintf(stderr, "ha: handshake sin 101\n"); return -1; }
    return 0;
}

/* -------------------------- parseo de estados ---------------------------- */
/* localiza el final del objeto JSON que empieza justo tras 'p' (p apunta tras '{') */
static const char *obj_end(const char *p){
    int depth = 1, instr = 0;
    for (const char *q = p; *q; q++){
        if (instr){ if (*q=='\\'){ if (q[1]) q++; } else if (*q=='"') instr = 0; }
        else { if (*q=='"') instr = 1;
               else if (*q=='{') depth++;
               else if (*q=='}'){ if (--depth == 0) return q; } }
    }
    return NULL;
}
/* strstr acotado a [p,end) */
static const char *find_within(const char *p, const char *end, const char *needle){
    size_t nl = strlen(needle);
    if (!p || !end || end <= p) return NULL;
    for (const char *q = p; q + nl <= end; q++)
        if (!memcmp(q, needle, nl)) return q;
    return NULL;
}

/* aplica un mensaje HA a las entidades. dev 1 si cambió algo */
static int ha_apply(ha_client *c, const char *msg){
    int changed = 0;
    for (int i = 0; i < c->n; i++){
        char key[HA_ID_LEN + 4];
        snprintf(key, sizeof key, "\"%s\":{", c->ent[i].id);
        const char *p = strstr(msg, key);
        if (!p) continue;
        p += strlen(key);                 /* justo tras el '{' del objeto de la entidad */
        const char *end = obj_end(p);
        if (!end) continue;
        /* estado nuevo: primer "s":"..." dentro del objeto (en "a" o en "+") */
        const char *sp = find_within(p, end, "\"s\":\"");
        if (sp){
            sp += 5; char st[24]; int k = 0;
            while (*sp && *sp != '"' && k < (int)sizeof st - 1) st[k++] = *sp++;
            st[k] = 0;
            if (!c->ent[i].known || strcmp(c->ent[i].state, st) != 0){
                strncpy(c->ent[i].state, st, sizeof c->ent[i].state - 1);
                c->ent[i].state[sizeof c->ent[i].state - 1] = 0;
                c->ent[i].known = 1; changed = 1;
            }
        } else if (!c->ent[i].known){
            c->ent[i].known = 1;          /* visto pero sin "s" (diff de atributos) */
        }
        /* brillo opcional */
        const char *bp = find_within(p, end, "\"brightness\":");
        if (bp){
            int b = atoi(bp + 13);
            if (b != c->ent[i].brightness){ c->ent[i].brightness = b; changed = 1; }
        }
    }
    return changed;
}

/* --------------------------------- API ----------------------------------- */
int ha_watch(ha_client *c, const char *entity_id){
    if (c->n >= HA_MAX_ENT) return -1;
    strncpy(c->ent[c->n].id, entity_id, HA_ID_LEN - 1);
    c->ent[c->n].id[HA_ID_LEN - 1] = 0;
    c->ent[c->n].state[0] = 0;
    c->ent[c->n].brightness = -1;
    c->ent[c->n].known = 0;
    c->n++;
    return 0;
}

ha_entity *ha_get(ha_client *c, const char *entity_id){
    for (int i = 0; i < c->n; i++)
        if (!strcmp(c->ent[i].id, entity_id)) return &c->ent[i];
    return NULL;
}

void ha_close(ha_client *c){
    if (c->fd >= 0) close(c->fd);
    c->fd = -1; c->ready = 0; c->rxlen = 0;
}

int ha_connect(ha_client *c, const char *ip, int port, const char *token){
    strncpy(c->ip, ip, sizeof c->ip - 1); c->ip[sizeof c->ip - 1] = 0;
    c->port = port; c->token = token;
    c->rxlen = 0; c->ready = 0; c->next_id = 10;

    c->fd = tcp_connect(ip, port);
    if (c->fd < 0){ fprintf(stderr, "ha: no conecta %s:%d\n", ip, port); return -1; }

    struct timeval tv = {8, 0};           /* timeout de setup */
    setsockopt(c->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);

    if (ws_handshake(c->fd, ip, port) < 0){ ha_close(c); return -1; }

    char msg[16384]; int n;
    n = read_text_message(c, msg, sizeof msg);                 /* auth_required */
    if (n < 0){ ha_close(c); return -1; }

    char auth[512];
    int al = snprintf(auth, sizeof auth, "{\"type\":\"auth\",\"access_token\":\"%s\"}", token);
    if (ws_send_text(c->fd, auth, al) < 0){ ha_close(c); return -1; }

    n = read_text_message(c, msg, sizeof msg);                 /* auth_ok | auth_invalid */
    if (n < 0){ ha_close(c); return -1; }
    if (!strstr(msg, "auth_ok")){ fprintf(stderr, "ha: auth rechazada: %.80s\n", msg); ha_close(c); return -1; }

    /* subscribe_entities con todas las vigiladas */
    char sub[1024]; int off = snprintf(sub, sizeof sub,
        "{\"id\":%d,\"type\":\"subscribe_entities\",\"entity_ids\":[", c->next_id++);
    for (int i = 0; i < c->n && off < (int)sizeof sub - 4; i++)
        off += snprintf(sub + off, sizeof sub - off, "%s\"%s\"", i ? "," : "", c->ent[i].id);
    off += snprintf(sub + off, sizeof sub - off, "]}");
    if (ws_send_text(c->fd, sub, off) < 0){ ha_close(c); return -1; }

    /* a partir de aquí, no bloqueante: la "a" y los diffs los recoge ha_poll */
    int fl = fcntl(c->fd, F_GETFL, 0);
    fcntl(c->fd, F_SETFL, fl | O_NONBLOCK);
    c->ready = 1; c->last_rx = time(0);
    fprintf(stderr, "ha: conectado y suscrito (%d entidades)\n", c->n);
    return 0;
}

int ha_poll(ha_client *c){
    if (c->fd < 0) return -1;
    /* drenar el socket (no bloqueante) */
    for (;;){
        if (c->rxlen >= (int)sizeof c->rx) return -1;   /* lleno sin poder extraer -> caída */
        int r = recv(c->fd, c->rx + c->rxlen, sizeof c->rx - c->rxlen, 0);
        if (r > 0){ c->rxlen += r; c->last_rx = time(0); continue; }
        if (r == 0) return -1;                           /* peer cerró */
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        if (errno == EINTR) continue;
        return -1;
    }
    int changed = 0;
    char msg[16384]; int plen, op;
    for (;;){
        int e = extract_frame(c, (unsigned char*)msg, sizeof msg, &plen, &op);
        if (e < 0) return -1;
        if (e == 0) break;
        if (op == 0x9){ ws_send_frame(c->fd, 0xA, (unsigned char*)msg, plen); continue; } /* ping->pong */
        if (op == 0x8) return -1;                         /* close */
        if (op != 0x1 && op != 0x2) continue;
        if (ha_apply(c, msg)) changed = 1;
    }
    return changed;
}

int ha_call(ha_client *c, const char *domain, const char *service, const char *entity_id){
    if (c->fd < 0 || !c->ready) return -1;
    char js[320];
    int n = snprintf(js, sizeof js,
        "{\"id\":%d,\"type\":\"call_service\",\"domain\":\"%s\",\"service\":\"%s\","
        "\"target\":{\"entity_id\":\"%s\"}}", c->next_id++, domain, service, entity_id);
    return ws_send_text(c->fd, js, n);
}

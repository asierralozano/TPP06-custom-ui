#include "fonts.h"
#include "manrope_ttf.h"
#include <stdio.h>
#include <stdlib.h>

/* Datos de la fuente: por defecto Manrope incrustada. Se puede sobrecargar con
   la variable de entorno TTF_FILE=<ruta> (para pruebas en el simulador). */
static const unsigned char *s_data = 0;
static unsigned int s_len = 0;

static void ensure_data(void){
    if(s_data) return;
    const char *ov = getenv("TTF_FILE");
    if(ov){
        FILE *f = fopen(ov, "rb");
        if(f){
            fseek(f,0,SEEK_END); long n=ftell(f); fseek(f,0,SEEK_SET);
            unsigned char *b = malloc(n>0?n:1);
            if(b && n>0 && fread(b,1,n,f)==(size_t)n){ s_data=b; s_len=(unsigned)n; fclose(f); return; }
            if(b) free(b);
            fclose(f);
        }
    }
    s_data = manrope_ttf;
    s_len  = manrope_ttf_len;
}

static lv_font_t *g_mr_font[64];
static int g_mr_sz[64];
static int g_mr_n = 0;

lv_font_t *manrope(int size){
    int i;
    for(i=0;i<g_mr_n;i++) if(g_mr_sz[i]==size) return g_mr_font[i];
    ensure_data();
    /* FIX bug LVGL 8.3: lv_lru_create hace hash_table_size = cache_size/(size*size);
       con el default 4096 y size>=65 da 0 buckets -> SIGBUS en lv_lru_get.
       Pasamos un cache_size que garantice >=16 buckets. */
    int avg = size*size;
    int cache = avg*16; if(cache < 4096) cache = 4096;
    lv_font_t *f = lv_tiny_ttf_create_data_ex(s_data, s_len, size, cache);
    if(f && g_mr_n<64){ g_mr_font[g_mr_n]=f; g_mr_sz[g_mr_n]=size; g_mr_n++; }
    return f;
}

#include <stdio.h>
#include "lvgl/lvgl.h"
#include "fonts.h"
int main(void){
    lv_init();
    printf("lv_init ok\n"); fflush(stdout);
    lv_font_t *f = manrope(40);
    printf("manrope(40) = %p\n", (void*)f); fflush(stdout);
    if(f){
        lv_font_glyph_dsc_t dsc;
        bool ok = lv_font_get_glyph_dsc(f, &dsc, '6', 0);
        printf("glyph '6' ok=%d adv_w=%d\n", (int)ok, (int)dsc.adv_w); fflush(stdout);
    }
    printf("done\n"); fflush(stdout);
    return 0;
}

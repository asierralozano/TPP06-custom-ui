#include "assets.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef ASSET_DEFAULT_DIR
#define ASSET_DEFAULT_DIR "/tuya/data/custom/assets"
#endif

const char *asset(const char *rel){
    static char buf[8][300];
    static int idx = 0;
    const char *base = getenv("ASSET_DIR");
    if(!base) base = ASSET_DEFAULT_DIR;
    char *b = buf[idx & 7];
    idx++;
    snprintf(b, sizeof(buf[0]), "A:%s/%s", base, rel);
    return b;
}

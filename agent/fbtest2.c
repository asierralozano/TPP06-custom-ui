/* fbtest2 — prueba la secuencia EXACTA de Tuya (tkl_disp_flush):
 * doble buffer en smem (buf0 @0, buf1 @smem/2) + pan por FBIOPUT_VSCREENINFO
 * alternando yoffset 0<->480. Parpadea ROJO/NEGRO 8 veces. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, uint32_t)
#endif

int main(void){
    int fd=open("/dev/fb0",O_RDWR);
    if(fd<0){ perror("fb0"); return 1; }
    struct fb_var_screeninfo var; struct fb_fix_screeninfo fix;
    ioctl(fd,FBIOGET_VSCREENINFO,&var);
    ioctl(fd,FBIOGET_FSCREENINFO,&fix);
    int W=var.xres,H=var.yres,STR=fix.line_length,LEN=STR*H;
    size_t SMEM=fix.smem_len?fix.smem_len:(size_t)LEN;
    int half_lines = (SMEM/2)/STR;   /* = 480 */
    fprintf(stderr,"W=%d H=%d STR=%d LEN=%d SMEM=%zu half_lines=%d yvirt=%d\n",
            W,H,STR,LEN,SMEM,half_lines,var.yres_virtual);
    /* intentar ampliar el espacio virtual para permitir el pan al buffer 1 */
    { struct fb_var_screeninfo v=var; v.yres_virtual=2*H; v.yoffset=0; v.activate=FB_ACTIVATE_NOW;
      int r=ioctl(fd,FBIOPUT_VSCREENINFO,&v);
      fprintf(stderr,"set yvirt=960 -> r=%d errno=%d yvirt_now=%d\n",r,errno,v.yres_virtual);
      if(r==0) var=v; }
    uint8_t *mm=mmap(NULL,SMEM,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(mm==MAP_FAILED){ perror("mmap"); return 1; }
    { int b=open("/sys/class/backlight/backlight/bl_power",O_WRONLY); if(b>=0){ if(write(b,"0\n",2)<0){} close(b);}
      b=open("/sys/class/backlight/backlight/brightness",O_WRONLY); if(b>=0){ if(write(b,"220\n",4)<0){} close(b);} }
    uint8_t *red=malloc(LEN),*blk=malloc(LEN);
    for(int i=0;i<LEN;i+=2){ red[i]=0x1f; red[i+1]=0x00; blk[i]=0; blk[i+1]=0; }  /* rojo / negro BGR565 */
    size_t off[2]={0,(size_t)half_lines*STR};
    int yoff[2]={0,half_lines};
    int cur=0;
    for(int k=0;k<8;k++){
        uint8_t *c = (k&1)?blk:red;
        int tgt = cur^1;
        uint32_t a=0; ioctl(fd,FBIO_WAITFORVSYNC,&a);
        memcpy(mm+off[tgt], c, LEN);
        var.xoffset=0; var.yoffset=yoff[tgt]; var.activate=FB_ACTIVATE_NOW;
        int r=ioctl(fd,FBIOPUT_VSCREENINFO,&var);
        if(k<2) fprintf(stderr,"flip tgt=%d yoffset=%d -> r=%d errno=%d\n",tgt,yoff[tgt],r,errno);
        cur=tgt;
        sleep(1);
    }
    fprintf(stderr,"fin\n");
    return 0;
}

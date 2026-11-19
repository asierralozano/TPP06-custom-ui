/* fbtest — sonda de mecanismo de presentación del ak-lcd.
 * Prueba varios métodos de "volcado" al LCD, cada uno con un color distinto,
 * parpadeando color<->negro 3 veces. El usuario observa con qué color/método
 * la pantalla SÍ cambia. Así identificamos el present correcto sin adivinar.
 *
 * Orden y color (legende para el usuario):
 *   1 ROJO     = write()
 *   2 VERDE    = memcpy(mmap) + FBIO_WAITFORVSYNC
 *   3 AZUL     = memcpy(mmap) + FBIOPAN_DISPLAY(yoffset=0)
 *   4 AMARILLO = memcpy(mmap) + FBIOPUT_VSCREENINFO
 *   5 CIAN     = memcpy(mmap) + FBIOPUT_VSCREENINFO(activate=FORCE)
 *   6 MAGENTA  = memcpy(mmap) + FBIOBLANK(UNBLANK)
 *   7 BLANCO   = doble buffer: escribe buffer2 + FBIOPAN_DISPLAY(yoffset=H)
 */
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

static int fd;
static uint8_t *mm;
static int W,H,STR,LEN;
static size_t SMEM;
static struct fb_var_screeninfo var;
static struct fb_fix_screeninfo fix;

static void fillbuf(uint8_t *b, uint16_t c){
    for (int i=0;i<LEN;i+=2){ b[i]=c&0xff; b[i+1]=c>>8; }
}
static void w_write(uint8_t*b){ lseek(fd,0,SEEK_SET); int o=0; while(o<LEN){int w=write(fd,b+o,LEN-o); if(w<=0)break; o+=w;} }

static void present(int method, uint8_t*buf){
    switch(method){
    case 1: w_write(buf); break;
    case 2: memcpy(mm,buf,LEN); { uint32_t a=0; ioctl(fd,FBIO_WAITFORVSYNC,&a);} break;
    case 3: memcpy(mm,buf,LEN); var.xoffset=0; var.yoffset=0; ioctl(fd,FBIOPAN_DISPLAY,&var); break;
    case 4: memcpy(mm,buf,LEN); var.activate=FB_ACTIVATE_NOW; ioctl(fd,FBIOPUT_VSCREENINFO,&var); break;
    case 5: memcpy(mm,buf,LEN); var.activate=FB_ACTIVATE_FORCE; ioctl(fd,FBIOPUT_VSCREENINFO,&var); break;
    case 6: memcpy(mm,buf,LEN); ioctl(fd,FBIOBLANK,FB_BLANK_UNBLANK); break;
    case 7: { static int tog=0; size_t off=tog?(size_t)LEN:0;
              if (off+LEN<=SMEM) memcpy(mm+off,buf,LEN);
              var.xoffset=0; var.yoffset = tog?H:0; ioctl(fd,FBIOPAN_DISPLAY,&var); tog^=1; } break;
    }
}

int main(void){
    fd=open("/dev/fb0",O_RDWR);
    if(fd<0){ perror("fb0"); return 1; }
    ioctl(fd,FBIOGET_VSCREENINFO,&var);
    ioctl(fd,FBIOGET_FSCREENINFO,&fix);
    W=var.xres;H=var.yres;STR=fix.line_length;LEN=STR*H;SMEM=fix.smem_len?fix.smem_len:(size_t)LEN;
    fprintf(stderr,"W=%d H=%d STR=%d LEN=%d SMEM=%zu yvirt=%d\n",W,H,STR,LEN,SMEM,var.yres_virtual);
    /* intentar habilitar doble buffer (para el método 7) */
    { struct fb_var_screeninfo v2=var; v2.yres_virtual=2*H; v2.activate=FB_ACTIVATE_NOW;
      if(ioctl(fd,FBIOPUT_VSCREENINFO,&v2)==0){ var=v2; fprintf(stderr,"yvirt ahora %d\n",var.yres_virtual);} }
    mm=mmap(NULL,SMEM,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(mm==MAP_FAILED){ perror("mmap"); return 1; }
    /* backlight */
    { int b=open("/sys/class/backlight/backlight/bl_power",O_WRONLY); if(b>=0){ if(write(b,"0\n",2)<0){} close(b);}
      b=open("/sys/class/backlight/backlight/brightness",O_WRONLY); if(b>=0){ if(write(b,"220\n",4)<0){} close(b);} }
    uint16_t colors[7]={0x001F,0x07E0,0xF800,0xFFE0,0x07FF,0xF81F,0xFFFF};
    uint8_t *cbuf=malloc(LEN),*kbuf=malloc(LEN); fillbuf(kbuf,0x0000);
    for(int m=1;m<=7;m++){
        fillbuf(cbuf,colors[m-1]);
        fprintf(stderr,"=== metodo %d ===\n",m);
        for(int k=0;k<3;k++){ present(m,cbuf); sleep(1); present(m,kbuf); sleep(1); }
    }
    fprintf(stderr,"fin\n");
    return 0;
}

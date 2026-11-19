#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <linux/input.h>
int main(int argc,char**argv){
    int secs=argc>1?atoi(argv[1]):25;
    int f0=open("/dev/input/event0",O_RDONLY|O_NONBLOCK);
    int f1=open("/dev/input/event1",O_RDONLY|O_NONBLOCK);
    int mx=(f0>f1?f0:f1)+1;
    time_t t0=time(0); struct input_event ev;
    while(time(0)-t0<secs){
        fd_set r; FD_ZERO(&r); if(f0>=0)FD_SET(f0,&r); if(f1>=0)FD_SET(f1,&r);
        struct timeval tv={1,0};
        if(select(mx,&r,0,0,&tv)>0){
            if(f0>=0&&FD_ISSET(f0,&r)) while(read(f0,&ev,sizeof ev)==(int)sizeof ev) if(ev.type) printf("EV0 type=%d code=%d val=%d\n",ev.type,ev.code,ev.value);
            if(f1>=0&&FD_ISSET(f1,&r)) while(read(f1,&ev,sizeof ev)==(int)sizeof ev) if(ev.type) printf("EV1 type=%d code=%d val=%d\n",ev.type,ev.code,ev.value);
            fflush(stdout);
        }
    }
    return 0;
}

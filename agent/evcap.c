#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>
#include <linux/input.h>
int main(int argc,char**argv){
    int secs=argc>1?atoi(argv[1]):22;
    int fd=open("/dev/input/event0",O_RDONLY);
    if(fd<0){perror("open event0");return 1;}
    time_t t0=time(0); struct input_event ev;
    while(time(0)-t0<secs){
        fd_set r; FD_ZERO(&r); FD_SET(fd,&r);
        struct timeval tv={1,0};
        if(select(fd+1,&r,0,0,&tv)>0 && FD_ISSET(fd,&r)){
            if(read(fd,&ev,sizeof ev)==(int)sizeof ev && ev.type!=0)
                printf("type=%d code=%d value=%d\n",ev.type,ev.code,ev.value);
            fflush(stdout);
        }
    }
    return 0;
}

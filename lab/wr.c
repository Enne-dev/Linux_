#include"c.h"

int main(void){
    int fd, j, nread;
    char buf[512];

    if((fd=open("fifo", O_WRONLY|O_NONBLOCK))<0){
        printf("fifo open failed\\n");
        exit(1);
    }

    for(j=0;j<3;j++){
        nread = read(0,buf,512);
        write(fd, buf, nread);
    }

    exit(0);
}

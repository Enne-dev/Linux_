#include "c.h"


int main(void){
    int fd, n;
    char buf[512];

    mkfifo("fifo", 0600);
    fd = open("fifo", O_RDONLY); //********************주의할 것
    for(;;){
        n = read(fd, buf, 512);
        write(1, buf, n);
    }
}

#include "c.h"

int main(void){
	int fd,i;
	int *addr;	

	fd = open("temp",O_RDONLY|O_CREAT, 0600);
	addr = mmap(NULL, 10*sizeof(int),PROT_READ,MAP_SHARED,fd,0);

	sleep(5);
	for(i=0;i<5;i++){
		printf("%d\n",*(addr+i));
	}
	sleep(5);
	for(i=5;i<10;i++){
		printf("%d\n",*(addr+i));
	}
	exit(0);
}

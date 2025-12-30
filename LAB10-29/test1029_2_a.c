#include "c.h"

int main(void){
	int fd, i;
	int *addr;

	fd = open("temp", O_RDWR|O_CREAT, 0600);
	addr = mmap(NULL, 10*sizeof(int), PROT_WRITE, MAP_SHARED, fd, 0);
	
	ftruncate(fd, sizeof(int)*10);
	for (i=0;i<10;i++){
		scanf("%d",addr+i);
	}

	exit(0);
}

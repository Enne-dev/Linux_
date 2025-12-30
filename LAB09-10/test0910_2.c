#include "c.h"

int main(void) {
	int data=35, fd, i;

	fd = open("test2", O_WRONLY|O_CREAT, 0640);
	for (i=0;i<10;i++){
		write(fd, &data, sizeof(int));
	}

	return 0;
}	

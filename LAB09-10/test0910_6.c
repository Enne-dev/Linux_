#include "c.h"

int main(void) {
	int data[10] = {0}, fd, i;

	fd = open("test2", O_RDONLY);
	read(fd, data, 5*sizeof(int));
	for (i=0;i<10;i++) {
		printf("%d ", data[i]);
	}
	return 0;
}

#include "c.h"

int main(void) {
	char ch = 'K';
	int fd, i;

	fd = open("test1", O_WRONLY|O_TRUNC);
	for (i=0;i<3;i++) {
		write(fd, &ch, sizeof(char));
	}
	return 0;
}

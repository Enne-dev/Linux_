#include "c.h"

int main(void) {
	char ch = 'Z';
	int fd, i;

	fd = open("test1", O_WRONLY|O_APPEND);
	for (i=0;i<5;i++) {
		write(fd, &ch, sizeof(char));
	}

	return 0;
}

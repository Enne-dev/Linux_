#include "c.h"

int main(void) {
	struct stat buf;
	
	stat("data1", &buf);
	printf("mode : %o, link : %ld, size : %ld\n", buf.st_mode, buf.st_nlink, buf.st_size);

	return 0;
}


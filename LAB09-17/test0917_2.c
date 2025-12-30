#include "c.h"

int list(const char *name, const struct stat *status, int type) {

	if (type == FTW_D || type == FTW_DNR) {
		printf("%s : %d\n", name, status->st_size);
	}
	else if (type == FTW_F) {
		if (status->st_mode&S_IXUSR || 
				status->st_mode&S_IXGRP ||
					status->st_mode&S_IXOTH) {
			printf("%s : %d\n", name, status->st_size);
		}
	}

	return 0;
}

int main(int argc, char **argv) {
	
	ftw(argv[1], list, 1);
	printf("pid=%d, gid=%d, sid=%d\n", getpid(), getpgid(0), getsid(getpid()));

	return 0;
}	

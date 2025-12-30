#include "c.h"

int main(void) {
	
	char ch1[101];
	scanf("%s", ch1);
	struct stat buf;
	stat(ch1, &buf);
	struct stat sym_buf;
	lstat(ch1, &sym_buf);
	
	char real_file[101];
	readlink(ch1, real_file, 101);
	
	printf("access permission : %o\n", sym_buf.st_mode);
	printf("file size : %ld\n", sym_buf.st_size);
	printf("symbolic link name : %s\n", ch1);

	printf("access permission : %o\n", buf.st_mode);
	printf("file size : %ld\n", buf.st_size);
	printf("real file name : %s\n", real_file);




	return 0;
}

#include "c.h"

int main(void) {
	char ch = 'X';
	int i, fd;

	fd = open("data1", O_RDWR|O_CREAT, 0600);
	
	for (i=0;i<10;i++) {
		write(fd, &ch, sizeof(char));
	}
	
	lseek(fd, 0, SEEK_SET);
	write(fd, "Y", sizeof(char));

	lseek(fd, 0, SEEK_END);
	write(fd, "Y", sizeof(char));


	for (i=1;i<8;i+=2){
		lseek(fd, i, SEEK_SET);
		write(fd, "Z", sizeof(char));
	}
	


	lseek(fd, 14, SEEK_SET);
	write(fd, "T", sizeof(char));
	
	lseek(fd, -2, SEEK_END);
	write(fd, "S", sizeof(char));

	lseek(fd, -2, SEEK_CUR);
	write(fd, "W", sizeof(char));

	return 0;
}
	

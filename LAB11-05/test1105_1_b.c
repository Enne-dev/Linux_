#include "c.h"

int main(void){
	char f[2][3] = {"f1","f2"};
	int i, in, fd[2];

	fd[0] = open(f[0], O_RDONLY);
	fd[1] = open(f[1], O_WRONLY);

	for (;;){
		read(fd[0], &in, sizeof(int));
		printf("%d\n",in);
		if (in == -1) exit(0);
		in = in + 8;

		write(fd[1], &in, sizeof(int));

	}
	return 0;
}

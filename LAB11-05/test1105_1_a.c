#include "c.h"

int main(void){
	char f[2][3] = {"f1","f2"};
	int i,in,fd[2];

	mkfifo(f[0], 0600);
	mkfifo(f[1], 0600);

	fd[0] = open(f[0] O_WRONLY);
	fd[1] = open(f[1], O_RDONLY);

	for (;;){
		scanf("%d",&in);
		write(fd[0],&in, sizeof(int));

		if (in == -1) exit(0);
	
		read(fd[1],&in,sizeof(int));
		printf("%d\n",in);
	}
	return 0;
}

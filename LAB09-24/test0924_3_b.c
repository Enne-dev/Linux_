#include "c.h"

int main(int argc, char **argv){
	int i;

	for(i=0;i<5;i++){
		printf("pid=%d, gid=%d, sid=%d\n",getpid(),getpgid(0),getsid(getpid()));
		sleep(1);
	}
	exit(atoi(argv[1]));
}

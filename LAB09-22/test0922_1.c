#include "c.h"

void do_child(void) {
	printf("pid = %d, gid = %d, sid = %d\n", getpid(), getpgid(0),getsid(getpid()));
	exit(0);
}

	



int main(int argc, char **argv) {
	int num, i;
	pid_t pid;

	num = atoi(argv[1]);

	for (i=0;i<num;i++) {
		pid = fork();
		if (pid == 0) {
			do_child();
		}
	}

	for (i=0;i<num;i++){
		wait(0);
	}
		
	return 0;
}

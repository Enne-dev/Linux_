#include "c.h"

void do_child(int N){
	int i;
	for(i=0;i<5;i++){
		printf("pid=%d, gid=%d, sid=%d\n",getpid(),getpgid(0), getsid(getpid()));
		sleep(1);
	}
	exit(N);
}




int main(void) {

	int i, n, status;
	pid_t pid[3];

	for(i=0;i<3;i++){
		pid[i]=fork();
		if (pid[i]==0){
			do_child(i+1);
		}
	}
	
	for (i=2;i>=0;i--){
		n=waitpid(pid[i],&status,0);
		if (WIFEXITED(status)){
			printf("%d...... %d\n", n, WEXITSTATUS(status));
		}
	}
	exit(0);
}

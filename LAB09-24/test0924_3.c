#include "c.h"

int main(void){
	int i,status;
	pid_t pid;

	for(i=0;i<3;i++){
		pid=fork();
		if (pid==0&&i==0){
			execl("./test0924_3_a.c","test0924_3_a","1","abcde",(char*)0);
			exit(0);
		}
		else if (pid==0&&i==1){
			execl("./test0924_3_b.c","test0924_3_b","2",(char*)0);
			exit(0);
		}
		else if (pid==0){
			execl("./test0924_3_c.c","test0924_3_c","3",(char*)0);
			exit(0);
		}
	}
	
	for (i=0;i<3;i++){
		pid=wait(&status);
		if (WIFEXITED(status)){
			printf("%d......%d\n",pid, WEXITSTATUS(status));
		}
	}
	exit(0);
}

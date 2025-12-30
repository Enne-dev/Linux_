#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/stat.h>
#include<sys/types.h>
#include<unistd.h>
#include<dirent.h>
#include<fcntl.h>
#include<ftw.h>
#include<wait.h>
#include<signal.h>
#include<sys/mman.h>
#include<sys/time.h>
#include<sys/msg.h>


void catchusr(int);

void do_child(int n){
	int i,pid;
	static struct sigaction act;

	act.sa_handler = catchusr;
	sigaction(SIGUSR1, &act, NULL);

	printf("%d-th child is created...\n", i);
	pause(); //signal이 올 때까지 멈춰있음

	pid=getpid();
	for(i=0;i<3;i++){
		printf("child %d .... \n", pid);
	}

	exit(n);
}

int main(void){
	int i, cid, status;
	pid_t pid[3];

	for (i=0;i<3;i++){
		pid[i]=fork();
		if (pid[i]==0){
			do_child(i);
		}
	}


	for (i=0;i<3;i++){
		sleep(1);
		kill(pid[i], SIGUSR1);
	}
	for (i=0;i<3;i++){
		cid=wait(&status);
		printf("child id=%d, exit status=%d\n", cid, WEXITSTATUS(status));
	}

	exit(0);
}

void catchusr(int signo){
	printf("........ catch SIGUSR1 : %d\n",signo);
}	

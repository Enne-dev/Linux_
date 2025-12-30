#include "c.h"

void catchalarm(int);
void catchint(int);

int main(void){
	int i,num,sum=0;
	sigset_t mask;
	static struct sigaction act1, act2;
	
	act1.sa_handler=catchalarm;
	sigaction(SIGALRM, &act1, NULL);

	act2.sa_handler=catchint;
	sigaction(SIGINT, &act2, NULL);

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);

	for (i=0;i<10;i++){
		sigprocmask(SIG_SETMASK,&mask,NULL);
		do{
			alarm(10);
		}while(scanf("%d",&num)<0);
		sigprocmask(SIG_UNBLOCK,&mask,NULL);
		alarm(0);
		sum+=num;
		printf("sum=%d\n",sum);
	}

	exit(0);
}

void catchalarm(int signo){
	printf("input !!! input !!! input !!!\n");
}

void catchint(int signo){
	printf("Do not interrupt !!!\n");
}

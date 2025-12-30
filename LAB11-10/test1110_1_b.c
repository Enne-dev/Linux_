#include "c.h"

void catchsig(int n);

int main(void){
	int shmid1,shmid2,i,n,*buf,*pid;
	key_t key1,key2;
	static struct sigaction act;

	act.sa_handler = catchsig;
	sigaction(SIGUSR1, &act, NULL);
	
	key1 = ftok("key",1);
	shmid1 = shmget(key1,sizeof(pid_t),0600|IPC_CREAT);
	pid=(int*)shmat(shmid1,0,0);
	*pid = getpid();

	key2=ftok("key",2);
	shmid2 = shmget(key2,10*sizeof(int),0600|IPC_CREAT);
	buf=(int*)shmat(shmid2,0,0);

	for (i=0;i<10;i++) {
		pause();
		printf("%d\n",*(buf+i));
	}
	shmctl(shmid1,IPC_RMID,0);
	shmctl(shmid2,IPC_RMID,0);
	
	exit(0);

}

void catchsig(int n){
}

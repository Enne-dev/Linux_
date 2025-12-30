#include "c.h"

int main(void){
	int shmid1, shmid2, i, n, *buf, *pid;
	key_t key1, key2;

	key1 = ftok("key", 1);
	shmid1 = shmget(key1,sizeof(pid_t), 0600 | IPC_CREAT);
	pid =(int*)shmat(shmid1,0,0);
	
	key2 = ftok("key", 2);
	shmid2 = shmget(key2,10*sizeof(int),0600|IPC_CREAT);
	buf=(int*)shmat(shmid2,0,0);

	while(*pid==0);
	printf("writer id = %id\n",*pid);

	for (i=0;i<10;i++){
		scanf("%d",(buf+i));
		kill(*pid, SIGUSR1);
	}
	exit(0);
}

	

#include "c.h"

void do_child(int fd){
	int in;
	pid_t pid = getpid();
	struct flock lock;

	//record locking
	lock.l_whence=SEEK_SET;
	lock.l_start=0;
	lock.l_len=4;
	lock.l_type=F_WRLCK;
	fcntl(fd, F_SETLKW, &lock);

	//정수 읽기
	lseek(fd,0,SEEK_SET);
	read(fd, &in,sizeof(int));
	printf("%ld reads %d ... \n",pid,in);
	sleep(5);

	in = in+10;
	//정수 다시 쓰기
	lseek(fd,0,SEEK_SET);
	write(fd, &in, sizeof(int));
	printf("%ld writes %d ... \n",pid,in);

	//record unlocking
	lock.l_type=F_UNLCK;
	fcntl(fd,F_SETLK,&lock);
	
	exit(0);
}

int main(void){
	int i,in,fd;
	pid_t pid;

	fd=open("data1", O_RDWR|O_CREAT,0600);
	scanf("%d",&in);
	write(fd,&in,sizeof(int));

	for(i=0;i<3;i++){
		pid=fork();
		if (pid==0) do_child(fd);
	}
	for(i=0;i<3;i++) wait(0);

	//정수 읽기
	lseek(fd,0,SEEK_SET);
	read(fd,&in,sizeof(int));
	printf("%d\n",in);
	
	exit(0);
}

#include "c.h"

int main(void){
	int i,in,fd,turn,n;
	pid_t pid;
	struct flock lock;

	fd=open("turn1",O_RDWR|O_CREAT,0600);

	//동기화를 위한 locking 연산
	

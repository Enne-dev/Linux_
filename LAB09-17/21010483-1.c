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


int list(const char *name, const struct stat *status, int type) {
	int cnt;
	struct dirent *d;
	DIR *dp;

	if (type == FTW_D) {
		dp = opendir(name);
		cnt = 0;
		while (d = readdir(dp)){
			cnt++;
		}
		if (cnt <= 2){
			printf("empty directory : %s \n", name);
		}
	}

	return 0;
}

int main(void) {

	ftw(".", list, 1);
	
	return 0;
}	

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

int main(void){
	time_t raw_time;
	struct tm *time_info;
	char dir_name[15];

	time(&raw_time);
	time_info = localtime(&raw_time);

	sprintf(dir_name, "LAB%02d-%02d", time_info->tm_mon+1, time_info->tm_mday);

	if (mkdir(dir_name, 0755)==0){
		printf("성공: '%s' 디렉토리를 생성했습니다.\n",dir_name);
	}
	else{
		perror("오류");
		return 1;
	}
	return 0;
}

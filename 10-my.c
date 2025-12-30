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
	else { 
		switch(errno) {//파일 생성 실패한 상황에 대해 여러 케이스로 나눔
			case EEXIST://추가 : 이미 존재할 경우
				fprintf(stderr, "오류: '%s' 디렉토리가 이미 존재합니다.\n",dir_name);
				break;
			case EACCES://추가 : 권한 없음
				fprintf(stderr, "오류: 디렉토리를 만들 권한이 없습니다.\n");
				break;
			case ENOENT://추가 : 상위 경로가 존재하지 않을 경우
				fprintf(stderr, "오류: 상위 경로가 존재하지 않습니다.\n");
				break;
			case ENOSPC://추가 : 디스크 공간 부족
				fprintf(stderr, "오류: 디스크 공간이 부족합니다.\n");
				break;
			case EROFS://추가 : 읽기 전용 파일 시스템
				fprintf(stderr, "오류: 읽기 전용 파일시스템입니다.\n");
				break;
			default://나머지 케이스 : mkdir 실패 오류 메세지 출력
				perror("mkdir 실패");
				break;
		}
	}
	
	return 0;
}

/* [구현 한계 및 보완점에 대한 고찰]
 * 1. chdir() 구현의 한계:
 * 본래 이 명령어의 최종 목표는 날짜 디렉터리 생성 후 'cd'처럼 해당 디렉터리로
 * 즉시 이동하는 것이었으나, 자식 프로세스가 부모 셸의 환경(현재 작업 디렉터리)을
 * 변경할 수 없는 리눅스 프로세스 격리 원칙에 따라 C 프로그램만으로는 구현이
 * 불가능함을 확인했습니다. 이에 대한 올바른 해결책은 셸 함수(Shell Function)를
 * 사용하는 것입니다.
 * 
 * 2. 오류 처리 개선:
 * mkdir() 실패 시, 단일 perror() 호출은 사용자에게 불친절하다고 판단했습니다.
 * 따라서 mkdir()을 한 번만 호출하고, 실패했을 경우 전역 변수 errno 값을 분석하여
 * '이미 존재함(EEXIST)', '권한 없음(EACCES)' 등 구체적인 원인을 사용자에게
 * 알려주는 방식으로 코드를 개선하여 안정성과 사용자 경험을 향상시켰습니다.
 * */

#ifndef MYIO4_H
#define MYIO4_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <errno.h>

#define SHM_TOTAL_SIZE (4096 * 16)
#define HEADER_SIZE 8

typedef struct {
    int nscanf_total;
    int nbytes_total;
    unsigned char data[SHM_TOTAL_SIZE - HEADER_SIZE];
} shared_data_t;

static int mode = 0;
static int nscanf_running = 0;
static int nscanf_target = 0;
static int nbytes_running = 0;
static int block_printf = 0;
static int shmid = -1;
static shared_data_t *shm_data = NULL;
static int is_parent = 0;

static void r_init(void) {
    shmid = shmget(IPC_PRIVATE, SHM_TOTAL_SIZE, IPC_CREAT | 0600);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    
    shm_data = (shared_data_t*)shmat(shmid, NULL, 0);
    if (shm_data == (void*)-1) {
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        exit(1);
    }
    //0. 공유메모리 붙히기
    memset(shm_data, 0, sizeof(shared_data_t));
    is_parent = 1;
    
    while (1) {
        pid_t pid = fork();
        
        if (pid < 0) {
            perror("fork");
            shmdt(shm_data);
            shmctl(shmid, IPC_RMID, NULL);
            exit(1);
        }
        
        if (pid == 0) {//자식 시작
       signal(SIGINT,SIG_DFL);//시그널 헨들러 설정
            is_parent = 0;
            
            if (shm_data->nscanf_total > 0) {//복구모드 확인 - 기존 fd 대체
                mode = 1;
                nscanf_running = 0;
                nbytes_running = 0;
                
                printf("입력번호는 %d; 원하는 입력번호는?\n", shm_data->nscanf_total);//복구번호 출력
            
                if (scanf("%d", &nscanf_target) != 1) nscanf_target = 0;//복구번호입력
                printf("최종 복구 번호는 %d 입니다.\n", nscanf_target);
                if (nscanf_target < 0) nscanf_target = 0;
                if (nscanf_target > shm_data->nscanf_total) nscanf_target = shm_data->nscanf_total;
                if (nscanf_target==0) {//목표 복구번호 설정
                    mode = 0;
                    shm_data->nscanf_total = 0;
                    shm_data->nbytes_total = 0;
                    printf("%d번째 입력까지 복구 완료.\n", nscanf_target);
                }
            } 
            else {//복구모드 아님
                mode = 0;
                nscanf_target = 0;
            }
            
            return;
        }//자식 부분 끝
        else if(pid > 0){//부모 시작
            signal(SIGINT,SIG_IGN);//부모: sigint 무시
            int status;
            waitpid(pid, &status, 0);
            if (WIFSIGNALED(status)) {
                //printf("\n[부모] 자식 프로세스 시그널로 종료 감지. 재시작합니다...\n\n"); 불필요한 출력
                sleep(1);
                continue;
            }
           else if (WIFEXITED(status)) {
                int exit_code = WEXITSTATUS(status);
                if (exit_code == 0) {
                    shmdt(shm_data);
                    shmctl(shmid, IPC_RMID, NULL);
                    exit(0);
                }
            }
        }//부모 종료
    }
}

static int r_scanf(const char *format, void *ptr) {
    if (format == NULL || ptr == NULL) return 0;
    if (shm_data == NULL) return 0;
    int size;
    int typ;

    if (strchr(format, 'c')) { typ = 1; size = 1; }
    else if (strchr(format, 'f')) { typ = 3; size = 4; }
    else if (strchr(format, 'd')) { typ = 2; size = 4; }
    else return 0;

    if (mode == 1 && nscanf_running < nscanf_target) {
        if ((nbytes_running + size) > shm_data->nbytes_total) return 0;
        memcpy(ptr, shm_data->data + nbytes_running, size);
        nbytes_running += size;
        nscanf_running += 1;
        
        if (nscanf_running >= nscanf_target) {
            mode = 0;
            shm_data->nbytes_total = nbytes_running;
            nbytes_running = 0;
            shm_data->nscanf_total = nscanf_target;
       nscanf_running = 0;
       block_printf = 1; 
        }
        return 1;
    }
    if(block_printf == 1) {
   block_printf = 0;
   printf(" ... recovery completed ...\n");
    }
    if (typ == 1) {
        char tmp;
        scanf(" %c", &tmp);
        *(char*)ptr = tmp;
        shm_data->data[shm_data->nbytes_total] = (unsigned char)tmp;
        shm_data->nbytes_total += 1;
        shm_data->nscanf_total += 1;
    } else if (typ == 2) {
        int tmp;
        scanf("%d", &tmp);
        *(int*)ptr = tmp;
        memcpy(shm_data->data + shm_data->nbytes_total, &tmp, 4);
        shm_data->nbytes_total += 4;
        shm_data->nscanf_total += 1;
    } else if (typ == 3) {
        float tmp;
        scanf("%f", &tmp);
        *(float*)ptr = tmp;
        memcpy(shm_data->data + shm_data->nbytes_total, &tmp, 4);
        shm_data->nbytes_total += 4;
        shm_data->nscanf_total += 1;
    }

    return 1;
}

static int r_printf(const char *format, ...) {
    if (format == NULL) return 0;
    if (mode==1||block_printf==1) return 0;

    va_list ap;
    va_start(ap, format);
    int r = vprintf(format, ap);
    va_end(ap);

    return r;
}

static void r_cleanup(void) {
    if (!is_parent && shm_data != NULL) {
        shm_data->nscanf_total = 0;
        shm_data->nbytes_total = 0;
        memset(shm_data->data, 0, sizeof(shm_data->data));
        
        shmdt(shm_data);
        shm_data = NULL;
    }
}

#endif

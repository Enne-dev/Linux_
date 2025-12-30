#ifndef MYIO4_H
#define MYIO4_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // fork, execl, _exit
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/wait.h>   // [추가] wait 함수 사용

/* --- 설정 --- */
// [중요] 컴파일 후 생성될 실행 파일의 이름과 일치해야 합니다.
// 업로드하신 파일명이 'a-test'이므로 설정해두었습니다.
#define EXEC_NAME "./a-test" 

#define SHM_KEY_PATH "."
#define SHM_KEY_ID   'R'
#define LOG_FILE_NAME "myio_recovery.log"
#define MAX_LOG_BYTES 65536

typedef struct {
    int init_done;
    int nscanf_total;
    int recovery_target;
    int log1_size;
    int scanf_bytes_offset;
    char log1_data[MAX_LOG_BYTES];
} SHM;

static SHM *g_shm_ptr = NULL;
static int g_shm_id = -1;
static int g_current_scanf_index = 0;

/* --- 함수 원형 --- */
static inline void handle_SIGINT(int sig);
static inline SHM* get_shm_singleton();
static inline void r_init();
static inline int r_scanf(const char *format, ...);
static inline void r_printf(const char *format, ...);
static inline void r_cleanup();

/* * [수정됨] 시그널 핸들러
 * Watcher 패턴에서는 자식이 죽어야 부모가 재시작하므로,
 * execl 대신 _exit(0)으로 종료합니다.
 */
static inline void handle_SIGINT(int sig) {
    if (g_shm_ptr == NULL) _exit(0);

    int fd_log1 = open(LOG_FILE_NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd_log1 != -1) {
        write(fd_log1, &g_shm_ptr->nscanf_total, sizeof(int));
        write(fd_log1, &g_shm_ptr->log1_size, sizeof(int));
        write(fd_log1, g_shm_ptr->log1_data, g_shm_ptr->log1_size);
        close(fd_log1);
    }
    
    // 자식 프로세스 종료 -> 부모의 wait()가 풀림 -> 부모가 재시작(exec)함
    _exit(0); 
}

static inline SHM* get_shm_singleton() {
    if (g_shm_ptr == NULL) {
        key_t key = ftok(SHM_KEY_PATH, SHM_KEY_ID);
        if (key == -1) { perror("ftok"); return NULL; }

        g_shm_id = shmget(key, sizeof(SHM), IPC_CREAT | 0666);
        if (g_shm_id < 0) { perror("shmget"); return NULL; }

        g_shm_ptr = (SHM *)shmat(g_shm_id, NULL, 0);
        if (g_shm_ptr == (void *)-1) { perror("shmat"); g_shm_ptr = NULL; return NULL; }
    }
    return g_shm_ptr;
}

/* * [핵심 변경] r_init: 감시자(Parent)와 일꾼(Child) 분리 
 */
static inline void r_init() {
    // 1. 프로세스 복제
    pid_t pid = fork();

    if (pid > 0) {
        // --- [부모 프로세스: 감시자] ---
        int status;
        // 자식이 죽을 때까지 대기 (Ctrl+C 등을 기다림)
        wait(&status); 
        
        // 자식이 죽으면 여기로 넘어옴.
        // 쉘로 나가지 않고, 프로그램을 처음부터 다시 실행 (부활)
        // 첫 번째 인자: 실행 파일 경로, 두 번째: argv[0] 이름
        execl(EXEC_NAME, EXEC_NAME + 2, NULL); // "./a-test", "a-test"
        
        exit(0); // exec 실패 시 종료
    }
    else if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    // --- [자식 프로세스: 일꾼] ---
    // 여기서부터는 기존 r_init 로직을 그대로 수행합니다.
    
    SHM *shm = get_shm_singleton();
    if (shm == NULL) exit(1);

    if (shm->init_done) return;
    
    shm->init_done = 1;
    shm->recovery_target = 0;
    shm->scanf_bytes_offset = 0;
    g_current_scanf_index = 0;

    signal(SIGINT, handle_SIGINT);

    int fd_log1 = open(LOG_FILE_NAME, O_RDONLY);
    
    if (fd_log1 != -1) {
        int nscanf_restored = 0;
        int log_data_bytes = 0;

        int count_read = read(fd_log1, &nscanf_restored, sizeof(int));
        int size_read = read(fd_log1, &log_data_bytes, sizeof(int));
        int data_read = read(fd_log1, shm->log1_data, MAX_LOG_BYTES);
        
        if (count_read > 0 && size_read > 0 && data_read > 0) {
            shm->nscanf_total = nscanf_restored;
            shm->log1_size = log_data_bytes;
            
            printf("입력번호는 %d; 원하는 입력번호는?\n", shm->nscanf_total);
            
            if (scanf("%d", &shm->recovery_target) != 1) {
                shm->recovery_target = 0;
            }
            if (shm->recovery_target < 0) shm->recovery_target = 0;
            if (shm->recovery_target > shm->nscanf_total) {
                shm->recovery_target = shm->nscanf_total;
            }
            
            printf("최종 복구 번호는 %d 입니다.\n", shm->recovery_target);

            char c;
            while ((c = getchar()) != '\n' && c != EOF); 
        }
        close(fd_log1);
    }

    if (shm->recovery_target == 0) {
        int init_flag = shm->init_done;
        memset(shm, 0, sizeof(SHM));
        shm->init_done = init_flag;
    }
}

static inline int r_scanf(const char *format, ...) {
    SHM *shm = get_shm_singleton();
    if (shm == NULL) return EOF;

    va_list args;
    va_start(args, format);

    g_current_scanf_index++;
    
    int recovery_mode = (g_current_scanf_index <= shm->recovery_target);
    int ret = 0;

    if (recovery_mode) {
        char* log_stream = (char*)shm->log1_data;
        char* current_pos = log_stream + shm->scanf_bytes_offset;
        char* end_of_log = log_stream + shm->log1_size;

        if (current_pos >= end_of_log) {
            ret = EOF;
        } else {
            ret = vsscanf(current_pos, format, args);
            char* newline = strchr(current_pos, '\n');
            if (newline && (newline < end_of_log)) {
                shm->scanf_bytes_offset = (newline - log_stream) + 1;
            } else {
                shm->scanf_bytes_offset = shm->log1_size;
            }
        }
    } else {
        char line_buffer[1024];
        if (fgets(line_buffer, sizeof(line_buffer), stdin) == NULL) {
            ret = EOF; 
        } else {
            int line_len = strlen(line_buffer);
            if (shm->log1_size + line_len <= MAX_LOG_BYTES) {
                memcpy(shm->log1_data + shm->log1_size, line_buffer, line_len);
                shm->log1_size += line_len;
                shm->nscanf_total++;
            }
            ret = vsscanf(line_buffer, format, args);
        }
    }
    va_end(args);
    return ret; 
}

static inline void r_printf(const char *format, ...) {
    SHM *shm = get_shm_singleton();
    if (shm == NULL) return;
    
    int recovery_mode = (g_current_scanf_index <= shm->recovery_target);
    if (recovery_mode) return;

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

static inline void r_cleanup() {
    SHM *shm = get_shm_singleton();
    if (shm == NULL) return;

    // 정상 종료 시 로그 파일 삭제 (이게 있어야 다음번엔 깨끗하게 시작)
    unlink(LOG_FILE_NAME);
    
    shmdt(shm);
    shmctl(g_shm_id, IPC_RMID, NULL);
    g_shm_ptr = NULL; 
}

#endif

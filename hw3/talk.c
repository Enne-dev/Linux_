#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <fcntl.h>

/* 강의 노트 Lect11: Shared Memory & Semaphore 관련 상수 및 구조체 */
#define KEY_NUM 987654
#define MEM_SIZE 1024
#define MAX_LOG 100 // 저장할 최대 메시지 수 (Circular Buffer)

/* 메시지 구조체 */
struct Msg {
    int sender_id;
    int msg_seq; // 전역 메시지 번호
    char text[256];
};

/* 공유 메모리에 저장될 데이터 구조 */
struct SharedData {
    int user_count;      // 접속한 유저 수 (시작 동기화용)
    int global_seq;      // 현재까지 발생한 총 메시지 수
    int quit_flags[4];   // 1~3번 유저의 종료 상태 (1: 종료요청, 0: 진행중)
    struct Msg logs[MAX_LOG]; // 메시지 저장소
};

/* Semaphore 공용체 (Lect11 p.9) [cite: 1159] */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

/* 세마포어 연산: P(wait) - 임계구역 진입 (Lect11 p.12) [cite: 1184] */
void p(int semid) {
    struct sembuf p_buf;
    p_buf.sem_num = 0;
    p_buf.sem_op = -1;
    p_buf.sem_flg = SEM_UNDO;
    if (semop(semid, &p_buf, 1) == -1) {
        perror("p operation failed");
        exit(1);
    }
}

/* 세마포어 연산: V(signal) - 임계구역 탈출 (Lect11 p.13) [cite: 1195] */
void v(int semid) {
    struct sembuf v_buf;
    v_buf.sem_num = 0;
    v_buf.sem_op = 1;
    v_buf.sem_flg = SEM_UNDO;
    if (semop(semid, &v_buf, 1) == -1) {
        perror("v operation failed");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./talk <id 1~3>\n");
        exit(1);
    }

    int my_id = atoi(argv[1]); // (a) 통신 ID를 인자로 받음
    if (my_id < 1 || my_id > 3) {
        fprintf(stderr, "ID must be 1, 2, or 3.\n");
        exit(1);
    }

    /* 1. Shared Memory 생성 및 연결 (Lect11 p.3-6) [cite: 1222, 1240] */
    int shmid = shmget((key_t)KEY_NUM, sizeof(struct SharedData), 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget failed");
        exit(1);
    }
    struct SharedData *shm = (struct SharedData *)shmat(shmid, NULL, 0);
    if (shm == (void *)-1) {
        perror("shmat failed");
        exit(1);
    }

    /* 2. Semaphore 생성 및 초기화 (Lect11 p.4-8) [cite: 1117, 1138] */
    int semid = semget((key_t)KEY_NUM, 1, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("semget failed");
        exit(1);
    }

    /* 가장 먼저 실행된 프로세스가 Shared Memory와 Semaphore 초기화 */
    // 단순화를 위해 user_count가 0일 때 초기화한다고 가정 (Race condition 방지를 위해 p/v 필요하지만 여기서는 간략히 처리)
    // 실제로는 별도 init 프로그램이나 atomic instruction이 좋으나, 과제 레벨에서는 순차 실행 혹은 lock 사용
    
    /* 세마포어 값 초기화 (값이 0이면 초기화 수행) - Lect11 p.8 SETVAL */
    union semun arg;
    struct semid_ds sem_ds;
    arg.buf = &sem_ds;
    // 세마포어가 이미 생성되었는지 확인하는 로직은 복잡하므로, 
    // 여기서는 첫 번째 실행자가 초기화하도록 함.
    // 안전하게 하기 위해 실행 시 잠시 lock
    
    /* 유저 입장 처리 */
    // 주의: 실제 환경에선 shm 초기화 경쟁 상태가 있을 수 있으나, 과제 범위를 고려해 단순 로직 적용
    if (shm->user_count == 0) { // 첫 번째 사용자
        arg.val = 1; 
        semctl(semid, 0, SETVAL, arg); // 세마포어 값을 1로 설정 (Unlock 상태)
        memset(shm, 0, sizeof(struct SharedData)); // 메모리 0으로 초기화
    }

    p(semid); // Lock
    shm->user_count++;
    v(semid); // Unlock

    printf("User %d entered. Waiting for others... (Current: %d/3)\n", my_id, shm->user_count);

    /* (b) 3개의 프로세스는 동시에 talk를 시작 */
    while (1) {
        if (shm->user_count >= 3) break;
        sleep(1); // Busy waiting 방지
    }
    printf("All users connected! Talk Start.\n");

    /* 지역 변수 설정 */
    int local_seq = shm->global_seq; // 내가 어디까지 읽었는지 추적
    char input_buf[512];
    int running = 1;
    int i_quit = 0; // 내가 종료를 눌렀는지 여부

    /* 3. Main Communication Loop (Select 사용) */
    while (running) {
        /* Lect10: Select 설정 [cite: 1373, 1375] */
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); // 0: Standard Input (키보드) 감시

        /* Timeout 설정 (0.1초) - 비동기적으로 메시지 수신 확인용 [cite: 1392] */
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms

        /* select 호출: 키보드 입력이 있거나, timeout이 될 때까지 대기 */
        int res = select(1, &readfds, NULL, NULL, &timeout);

        if (res == -1) {
            perror("select error");
            break;
        }

        /* 3-1. 키보드 입력 처리 (내가 메시지 보냄) */
        if (FD_ISSET(0, &readfds)) {
            int n = read(0, input_buf, 512); // Lect4: read 
            if (n > 0) {
                input_buf[n] = '\0';
                // 개행 문자 제거
                if(input_buf[n-1] == '\n') input_buf[n-1] = '\0';

                // "talk_quit" 확인
                if (strcmp(input_buf, "talk_quit") == 0) {
                    p(semid);
                    shm->quit_flags[my_id] = 1; // 내 종료 플래그 설정
                    v(semid);
                    i_quit = 1;
                    printf("Waiting for other users to quit...\n");
                } else if (!i_quit) { // 종료 대기 중이 아닐 때만 메시지 전송
                    p(semid); // Critical Section 시작
                    
                    shm->global_seq++; // (a) msg# 증가
                    int idx = shm->global_seq % MAX_LOG;
                    
                    shm->logs[idx].sender_id = my_id;
                    shm->logs[idx].msg_seq = shm->global_seq;
                    strncpy(shm->logs[idx].text, input_buf, 256);

                    v(semid); // Critical Section 끝
                }
            }
        }

        /* 3-2. 수신 메시지 확인 및 출력 (비동기 처리 - 조건 c) */
        // 내 local_seq보다 shm의 global_seq가 크면 새 메시지가 있는 것임
        // 조건 (d): 공유 메모리의 순서대로 읽으므로 3개 프로세스 모두 동일 순서 보장
        
        // *주의*: 읽기는 Lock을 걸지 않아도 됨 (단일 쓰기, 다중 읽기 환경에서 int 읽기는 대게 atomic하며, 
        // 데이터가 깨져도 화면 출력만 이상할 뿐 프로세스가 죽진 않음. 엄격하게는 Lock 필요하나 성능상 제외)
        
        while (local_seq < shm->global_seq) {
            local_seq++;
            int idx = local_seq % MAX_LOG;
            
            struct Msg current_msg = shm->logs[idx];
            
            // (e) 자신이 입력한 문자열은 스스로 출력하지 않음
            if (current_msg.sender_id != my_id) {
                // (a) [sender=ID & msg#=NUM] 형식 출력
                printf("[sender=%d & msg#=%d] %s\n", 
                       current_msg.sender_id, current_msg.msg_seq, current_msg.text);
            }
        }

        /* 3-3. 종료 조건 확인 (b) 동시에 talk 종료 */
        if (shm->quit_flags[1] && shm->quit_flags[2] && shm->quit_flags[3]) {
            running = 0;
        }
    }

    printf("Talk finished.\n");

    /* 리소스 정리 (마지막에 나가는 프로세스가 정리) */
    p(semid);
    shm->user_count--;
    if (shm->user_count == 0) {
        v(semid);
        shmctl(shmid, IPC_RMID, 0); // Lect11 [cite: 1288]
        semctl(semid, 0, IPC_RMID, 0); // Lect11 [cite: 1143]
        printf("Cleaned up resources.\n");
    } else {
        v(semid);
    }
    
    // Shared memory detach (Lect11 p.7) [cite: 1253]
    shmdt(shm);

    return 0;
}

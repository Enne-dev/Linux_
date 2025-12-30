#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>

/* --- 상수 및 정의 --- */
#define MAX_USERS 10       // 최대 N명
#define SHM_KEY 0x1111     // 공유 메모리 키
#define SEM_KEY 0x2222     // 세마포어 키

// 사용자 상태 상수
#define EMPTY 0
#define ACTIVE 1

/* --- 데이터 구조체 --- */

// 1. 개별 사용자 정보
typedef struct {
    int id;             // 유저 ID (0 ~ N-1)
    pid_t pid;          // 프로세스 ID (Main)
    int qid;            // 수신용 메시지 큐 ID
    int status;         // EMPTY or ACTIVE
} UserInfo;

// 2. 공유 메모리 전체 구조
typedef struct {
    int msg_seq;                // 전역 메시지 번호
    int user_count;             // 현재 접속자 수
    UserInfo users[MAX_USERS];  // 유저 관리 테이블
} ShmData;

// 3. 메시지 버퍼
typedef struct {
    long mtype;         // 항상 1
    int sender_id;      // 보낸 사람 ID
    int seq_num;        // 메시지 시퀀스 번호
    char text[256];     // 내용
} MsgBuf;

/* --- 전역 변수 (Signal Handler용) --- */
int shm_id = -1;
int sem_id = -1;
int my_qid = -1;
int my_id = -1;
ShmData *shm_ptr = NULL;
pid_t sender_pid = 0, receiver_pid = 0;

/* --- 세마포어 유틸리티 --- */
void p_sem() {
    struct sembuf buf = {0, -1, SEM_UNDO};
    if (semop(sem_id, &buf, 1) == -1) perror("p_sem error");
}

void v_sem() {
    struct sembuf buf = {0, 1, SEM_UNDO};
    if (semop(sem_id, &buf, 1) == -1) perror("v_sem error");
}

/* --- 정리(Cleanup) 함수 --- */
void cleanup(int signum) {
    // 자식 프로세스들 종료
    if (sender_pid > 0) kill(sender_pid, SIGKILL);
    if (receiver_pid > 0) kill(receiver_pid, SIGKILL);

    if (my_id != -1 && shm_ptr != NULL) {
        p_sem(); // 데이터 수정 전 잠금
        
        printf("\n[System] User %d leaving...\n", my_id);
        
        // 내 자리 비우기
        shm_ptr->users[my_id].status = EMPTY;
        shm_ptr->users[my_id].pid = 0;
        shm_ptr->users[my_id].qid = 0;
        shm_ptr->user_count--;
        
        // 내 메시지 큐 삭제
        if (my_qid != -1) {
            if (msgctl(my_qid, IPC_RMID, NULL) == -1) 
                perror("msgctl RMID failed");
        }

        v_sem(); // 잠금 해제

        // 마지막 유저라면 공유 자원 전체 삭제
        if (shm_ptr->user_count <= 0) {
            printf("[System] Last user left. Removing SHM & SEM.\n");
            shmctl(shm_id, IPC_RMID, NULL);
            semctl(sem_id, 0, IPC_RMID);
        } else {
            shmdt(shm_ptr); // 분리만 함
        }
    }
    exit(0);
}

/* --- Receiver 프로세스 --- */
void do_receiver() {
    MsgBuf msg;
    while (1) {
        // 내 큐(my_qid)에서 메시지를 읽음 (Blocking)
        if (msgrcv(my_qid, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1) {
            // 큐가 삭제되었거나 에러 시 종료
            exit(0);
        }
        
        // 내가 보낸 메시지가 아니면 출력 (Echo 방지)
        if (msg.sender_id != my_id) {
            printf("\r[sender=%d & msg#=%d] %s", msg.sender_id, msg.seq_num, msg.text);
            fflush(stdout);
        }
    }
}

/* --- Sender 프로세스 --- */
void do_sender() {
    char buffer[256];
    MsgBuf msg;
    msg.mtype = 1;
    msg.sender_id = my_id;

    while (1) {
        printf("ME: ");
        if (fgets(buffer, sizeof(buffer), stdin) == NULL) break; // EOF(Ctrl+D)

        // 메시지 복사
        strcpy(msg.text, buffer);

        /* == 임계 영역 시작 (메시지 번호 증가 & 전송) == */
        p_sem();

        // 1. 번호 따기
        shm_ptr->msg_seq++;
        msg.seq_num = shm_ptr->msg_seq;

        // 2. 브로드캐스트 (나 빼고 Active인 사람에게)
        for (int i = 0; i < MAX_USERS; i++) {
            if (i != my_id && shm_ptr->users[i].status == ACTIVE) {
                int target_qid = shm_ptr->users[i].qid;
                
                // 큐가 유효한지 체크 (선택사항이지만 안전을 위해)
                if (msgsnd(target_qid, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
                    // 전송 실패 시 처리 (무시)
                }
            }
        }
        v_sem();
        /* == 임계 영역 끝 == */
    }
    // 루프 탈출 시 (Ctrl+D) 부모에게 종료 알림
    exit(0);
}

/* --- Main 함수 --- */
int main() {
    // 시그널 등록 (Ctrl+C)
    signal(SIGINT, cleanup);

    // 1. 공유 메모리 생성/연결
    shm_id = shmget(SHM_KEY, sizeof(ShmData), IPC_CREAT | 0666);
    if (shm_id == -1) { perror("shmget"); exit(1); }
    shm_ptr = (ShmData *)shmat(shm_id, NULL, 0);

    // 2. 세마포어 생성/초기화
    // IPC_EXCL을 사용하여 처음 만드는 사람만 초기화하도록 함
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (sem_id != -1) {
        // 처음 생성됨 -> 값 1로 초기화
        semctl(sem_id, 0, SETVAL, 1);
        // 공유 메모리 데이터 초기화
        memset(shm_ptr, 0, sizeof(ShmData));
        printf("[System] Chat room created.\n");
    } else {
        if (errno == EEXIST) {
            // 이미 존재함 -> 연결만
            sem_id = semget(SEM_KEY, 1, 0666);
        } else {
            perror("semget"); exit(1);
        }
    }

    // 3. 로그인 (ID 할당) - 임계 영역
    p_sem();
    if (shm_ptr->user_count >= MAX_USERS) {
        printf("거절n");
        v_sem();
        shmdt(shm_ptr);
        exit(1);
    }

    // 빈 슬롯 찾기
    for (int i = 0; i < MAX_USERS; i++) {
        if (shm_ptr->users[i].status == EMPTY) {
            my_id = i;
            break;
        }
    }

    // 내 전용 메시지 큐 생성 (IPC_PRIVATE 사용)
    my_qid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    if (my_qid == -1) { perror("msgget"); v_sem(); exit(1); }

    // 유저 정보 등록
    shm_ptr->users[my_id].id = my_id;
    shm_ptr->users[my_id].pid = getpid();
    shm_ptr->users[my_id].qid = my_qid;
    shm_ptr->users[my_id].status = ACTIVE;
    shm_ptr->user_count++;

    printf("id=%d, talkers=%d, msg#=\n", my_id, shm_ptr->user_count);
    v_sem();
    /* --- 로그인 완료 --- */

    // 4. 프로세스 분기 (Sender / Receiver)
    
    // 첫 번째 Fork: Receiver
    receiver_pid = fork();
    if (receiver_pid == 0) {
        do_receiver(); // 자식 1은 수신만 담당
        exit(0); // 리턴되지 않음
    }

    // 두 번째 Fork: Sender
    sender_pid = fork();
    if (sender_pid == 0) {
        do_sender(); // 자식 2는 송신만 담당
        exit(0);
    }

    // 5. Main 프로세스 (Parent): 자식 종료 대기
    // Sender(입력창)가 종료(Ctrl+D)되면 같이 종료 처리
    waitpid(sender_pid, NULL, 0);
    
    // Sender가 죽으면 정리하고 나감
    cleanup(0);

    return 0;
}

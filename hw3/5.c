#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <errno.h>

/* =============================================================
 * 상수 및 구조체 정의 (talk.h 내용 포함)
 * ============================================================= */
#define KEY_NUM     12345
#define TOKEN_TYPE  999     // 토큰 메시지용 mtype
#define CHECKIN_TYPE 888    // 입장 확인용 mtype

struct MsgBuf {
    long mtype;       // 메시지 타입 (수신자 ID or TOKEN_TYPE)
    int sender_id;    // 보낸 사람 ID
    int seq_num;      // 메시지 순서 번호
    char text[512];   // 내용
};

/* =============================================================
 * Receiver 함수 (Child Process)
 * 역할: 내 ID로 된 메시지를 수신하여 비동기적으로 출력
 *
 * ============================================================= */
void receiver(int qid, int my_id) {
    struct MsgBuf msg;
    int len;

    while(1) {
        // 1. 내 ID로 온 메시지만 수신 (Blocking)
        len = msgrcv(qid, &msg, sizeof(struct MsgBuf) - sizeof(long), my_id, 0);
        if (len == -1) {
            // 큐가 삭제되었거나 에러 발생 시 종료
            if (errno == EIDRM) break; 
            perror("msgrcv failed");
            exit(1);
        }

        // 2. 종료 신호 확인 (b: 동시 종료)
        if (strcmp(msg.text, "talk_quit") == 0) {
            printf("\n[System] talk_quit received. Receiver terminating.\n");
            break; 
        }

        // 3. 메시지 출력 (a, d, e 조건 만족)
        if (msg.sender_id != my_id) {
            printf("[sender=%d & msg#=%d] %s\n", msg.sender_id, msg.seq_num, msg.text);
        }
    }
    exit(0);
}

/* =============================================================
 * Sender 함수 (Parent Process)
 * 역할: 키보드 입력 -> 토큰 획득(Lock) -> 순서할당 -> 전송 -> 토큰 반납
 *
 * ============================================================= */
void sender(int qid, int my_id) {
    struct MsgBuf msg_packet;
    struct MsgBuf token_packet;
    char input_buf[512];
    int len;

    while (1) {
        // 1. 키보드 입력 (Blocking)
        if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) break;
        input_buf[strcspn(input_buf, "\n")] = 0; // 개행 제거

        // 2. 토큰 획득 시도 (순서 제어를 위한 Lock)
        // 큐에 TOKEN_TYPE 메시지가 있을 때까지 대기(Block)
        len = msgrcv(qid, &token_packet, sizeof(struct MsgBuf) - sizeof(long), TOKEN_TYPE, 0);
        if (len == -1) {
            if (errno == EIDRM) break;
            perror("Token receive failed");
            exit(1);
        }

        // 3. 순서 번호(Sequence) 업데이트
        token_packet.seq_num++;
        int current_seq = token_packet.seq_num;

        // 4. 메시지 패킷 구성
        msg_packet.sender_id = my_id;
        msg_packet.seq_num = current_seq;
        strcpy(msg_packet.text, input_buf);

        // 5. 브로드캐스팅 (나를 제외한 나머지에게 전송)
        for (int i = 1; i <= 3; i++) {
            if (i == my_id) continue; // (e) 자신에게는 안 보냄
            
            msg_packet.mtype = i;
            if (msgsnd(qid, &msg_packet, sizeof(struct MsgBuf) - sizeof(long), 0) == -1) {
                perror("Message send failed");
            }
        }

        // 6. 종료 조건 처리 ("talk_quit" 입력 시)
        int is_quit = (strcmp(input_buf, "talk_quit") == 0);

        // 7. 토큰 반납 (Unlock)
        // 종료하더라도 토큰은 돌려줘야 다른 프로세스가 작동함 (혹은 정리됨)
        token_packet.mtype = TOKEN_TYPE;
        msgsnd(qid, &token_packet, sizeof(struct MsgBuf) - sizeof(long), 0);

        if (is_quit) {
            printf("[System] Quitting sender process...\n");
            break;
        }
    }
    // 부모 프로세스 종료 시 자식(receiver)에게 시그널을 보내거나 wait
}

/* =============================================================
 * Main 함수
 * 역할: 초기화, 동기화(3명 대기), fork() 분기
 * ============================================================= */
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: ./talk <id 1~3>\n");
        exit(1);
    }

    int my_id = atoi(argv[1]);
    if (my_id < 1 || my_id > 3) {
        fprintf(stderr, "ID must be 1, 2, or 3.\n");
        exit(1);
    }

    // 1. 메시지 큐 생성 또는 연결
    int qid = msgget((key_t)KEY_NUM, 0666 | IPC_CREAT);
    if (qid == -1) {
        perror("msgget failed");
        exit(1);
    }

    // 2. 입장 확인 (Synchronization) - 조건 (b) "동시 시작" 구현
    // "체크인" 메시지를 하나 보냄
    struct MsgBuf checkin_msg;
    checkin_msg.mtype = CHECKIN_TYPE;
    msgsnd(qid, &checkin_msg, 0, 0); // 내용은 필요 없음, header만

    printf("User %d ready. Waiting for others...\n", my_id);

    // 큐에 CHECKIN_TYPE 메시지가 3개 모일 때까지 대기
    struct msqid_ds q_stat;
    while (1) {
        msgctl(qid, IPC_STAT, &q_stat); // 큐 상태 확인
        // 단순화를 위해 전체 메시지 수로 체크 (엄격하게는 타입을 세야 함)
        // 여기서는 CHECKIN 메시지 외엔 없다고 가정
        if (q_stat.msg_qnum >= 3) break;
        sleep(1);
    }
    printf("All users connected! Starting Talk.\n");

    // 3. 토큰 초기화 (User 1만 수행)
    // 누군가는 최초의 토큰 1개를 넣어야 시스템이 돔.
    if (my_id == 1) {
        // 기존 체크인 메시지 청소 (선택 사항, 여기선 생략하고 토큰 투입)
        struct MsgBuf init_token;
        init_token.mtype = TOKEN_TYPE;
        init_token.seq_num = 0; // msg# 0번부터 시작
        init_token.sender_id = 0;
        
        // 토큰 투입 (이미 있으면 안됨, 간단 구현을 위해 1번만 수행)
        if (msgsnd(qid, &init_token, sizeof(struct MsgBuf) - sizeof(long), 0) == -1) {
            perror("Token init failed");
        }
    }

    // 4. 프로세스 분기 (Fork) - 조건 (c) 비동기 입출력 구현
    //
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    if (pid == 0) {
        // Child Process: Receiver 역할
        receiver(qid, my_id);
    } else {
        // Parent Process: Sender 역할
        sender(qid, my_id);
        
        // Sender가 루프를 빠져나오면(talk_quit), 자식 프로세스 정리
        kill(pid, SIGTERM); // 자식(Receiver) 강제 종료
        wait(NULL);         // 자식 종료 대기
        
        // 마지막 정리 (User 1이 대표로 큐 삭제)
        // 실제로는 제일 늦게 나가는 사람이 지워야 하나, 과제용으론 1번이 지움
        if (my_id == 1) {
            sleep(1); // 다른 프로세스들이 종료할 시간 확보
            msgctl(qid, IPC_RMID, NULL); //
            printf("Message Queue removed.\n");
        }
    }

    return 0;
}

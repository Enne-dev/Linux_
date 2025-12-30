#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_USERS 5
#define MAX_TEXT 512

// 공유 메모리에 저장될 데이터 구조
struct shared_data {
	int total;            // 현재 참여자 수
	int msgn;             // 현재 메시지 번호 (mtype으로 사용)
	int ids[MAX_USERS];   // ID 할당 배열 (0: 미사용, 1: 사용 중)
};

// 메시지 큐 버퍼 구조
struct msg_buf {
	long mtype;
	pid_t sender_id;
	char text[MAX_TEXT];
};

// 세마포어 연산 함수
void sem_op(int semid, int op) {
	struct sembuf sb = {0, op, SEM_UNDO};//kill -9 맞는거 방지
	semop(semid, &sb, 1);
}

void sender(int semid, struct shared_data *shm_ptr, int myid, int msgid);
void receiver(struct shared_data *shm_ptr, int myid, int msgid);

int main() {// 내 ID 번호를 알아야 함(talk에 참여를 하는 시점에)
	int semid, shmid, msgid, myid;
	int i;
	struct shared_data *shm_ptr;
	int fd = open("keyFile", O_RDWR|O_CREAT, 0666); // 키 파일 생성
	close(fd);
	
	key_t SEM_KEY = ftok("keyFile", 1);
	key_t SHM_KEY = ftok("keyFile", 2);
	key_t MSG_KEY = ftok("keyFile", 3);

	// semaphore 생성 및 초기화
	if ((semid = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0600)) != -1) {
		// 최초 생성자는 SemVal을 1로 초기화
		semctl(semid, 0, SETVAL, 1);
	} else {
		// 이미 생성된 경우 연결만
		semid = semget(SEM_KEY, 1, 0600);
	}

	// 공유 메모리 및 메시지 큐 연결
	shmid = shmget(SHM_KEY, sizeof(struct shared_data), IPC_CREAT | 0600);
	shm_ptr = (struct shared_data *)shmat(shmid, NULL, 0);
	msgid = msgget(MSG_KEY, IPC_CREAT | 0600);

	// 3. 참여자 수 체크 및 업데이트 (설계도 1-2)
	sem_op(semid, -1);  // !!Critical Section!!
	if (shm_ptr->total >= MAX_USERS) {  
		// 최대 참여자 수 초과라면 Semaphre 반납 후 해제
		printf("거절\n");
		sem_op(semid, 1); 
		shmdt(shm_ptr);
		exit(0);
	}
	shm_ptr->total++;

	for(i = 0; i < MAX_USERS; i++){
		if(shm_ptr->ids[i] == 0){
			shm_ptr->ids[i] = 1;
			myid = i + 1;
			break;
		}
	}
	printf("id=%d, talkers=%d, msg#=%d\n", myid, shm_ptr->total, shm_ptr->msgn);
	sem_op(semid, 1);   // !!End Critical Section!!

	// 4. Sender/Receiver 프로세스 분리 (설계도 1-3)
	pid_t pid = fork();
	if (pid == 0) {
		// 자식 프로세스 1: Receiver
		receiver(shm_ptr, myid, msgid);
	} else {
		pid_t pid2 = fork();
		if (pid2 == 0) {
			// 자식 프로세스 2: Sender
			sender(semid, shm_ptr, myid, msgid);
		} else {
			// 부모 프로세스: 종료 대기
			waitpid(pid, NULL, 0);
			waitpid(pid2, NULL, 0);

			// 종료 처리, 참여자 수 감소 및 ID 반납
			sem_op(semid, -1);
			shm_ptr->total--;
			shm_ptr->ids[myid - 1] = 0;
			if(shm_ptr->total == 0){
				// 마지막 참여자라면 공유 자원 해제
				shmctl(shmid, IPC_RMID, NULL);
				msgctl(msgid, IPC_RMID, NULL);
				semctl(semid, 0, IPC_RMID);
			}else{
				sem_op(semid, 1);
				shmdt(shm_ptr);
			}
		}
	}
	exit(0);
}

// 설계도 2: Sender 함수 구현
void sender(int semid, struct shared_data *shm_ptr, int myid, int msgid) {//현재 메시지 번호를 알아야 함, 현재 대화에 참여한 사용자 수를 알아야 함.
	struct msg_buf buf;
	int i;
	buf.sender_id = myid;

	while (1) {
		fgets(buf.text, MAX_TEXT, stdin);
		buf.text[strlen(buf.text) - 1] = '\0'; // 개행 제거

		sem_op(semid, -1); // 임계 구역 진입
		
		// 메시지 번호 관리 (99999 초과 시 0 초기화)
		if (shm_ptr->msgn >= 99999) shm_ptr->msgn = 0;
		else shm_ptr->msgn++;
		
		buf.mtype = shm_ptr->msgn;
		int current_total = shm_ptr->total;
		
		sem_op(semid, 1); // 임계 구역 탈출

		// 모든 참여자가 받을 수 있도록 total 횟수만큼 전송
		for (i = 0; i < current_total; i++) {
			msgsnd(msgid, &buf, sizeof(struct msg_buf) - sizeof(long), 0);
		}

        if (current_total == 1){
            printf("talker=1\n");
        }

		if (strcmp(buf.text, "talk_quit") == 0) {
			exit(0);
		}
	}
}

// 설계도 3: Receiver 함수 구현
void receiver(struct shared_data *shm_ptr, int myid, int msgid) {//내가 마지막으로 받은 seq 번호를 알아야 함
	struct msg_buf buf;
	int local_msgn = shm_ptr->msgn; // 초기 메시지 번호 설정

	while (1) {
		local_msgn++;
		if (local_msgn > 99999) local_msgn = 0;

		// 특정 메시지 번호(mtype)를 기다림 (에러 체크 제거)
		msgrcv(msgid, &buf, sizeof(struct msg_buf) - sizeof(long), local_msgn, 0);
		if(buf.sender_id != myid){
			printf("[sender=%d & msg=%d] %s\n", buf.sender_id, (int)buf.mtype, buf.text);
		}
		else if (strcmp(buf.text, "talk_quit") == 0) {
			exit(0);
		}
		
	}
}

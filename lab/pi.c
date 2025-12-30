#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>  // fcntl, O_NONBLOCK 사용을 위해 필수
#include <errno.h>  // errno, EAGAIN 사용을 위해 필수
#include <string.h>

int main() {
    int p[2];
    char buf[20];
    int nread;

    // 1. 파이프 생성
    if (pipe(p) == -1) {
        perror("pipe error");
        exit(1);
    }

    // 2. 부모/자식 분리
    if (fork() == 0) {
        // [자식 프로세스]
        close(p[0]); // 읽기 닫기
        
        printf("[자식] 3초 동안 딴짓하다가 보낼게요...\n");
        sleep(3); // 3초 대기 (부모를 애태우는 중)
        
        write(p[1], "Hello World!", 12);
        printf("[자식] 전송 완료! 죽습니다.\n");
        exit(0);
    } else {
        // [부모 프로세스]
        close(p[1]); // 쓰기 닫기

        // ★★★ 여기가 핵심! (파이프 속성 변경) ★★★
        // p[0](읽기용 파이프)를 Non-blocking 모드로 변경
        int flags = fcntl(p[0], F_GETFL, 0);      // 기존 설정 가져오기
        fcntl(p[0], F_SETFL, flags | O_NONBLOCK); // 기존 설정 + Non-blocking 추가 [cite: 68-69]

        // 3. 끈질기게 확인하기 (Polling)
        while (1) {
            memset(buf, 0, sizeof(buf)); // 버퍼 초기화
            nread = read(p[0], buf, sizeof(buf));

            if (nread == -1) {
                // 데이터가 없어서 실패한 경우 (EAGAIN)
                if (errno == EAGAIN) { // 
                    printf("[부모] 파이프 텅 비었네... 딴짓 하는 중...\n");
                    sleep(1); // 1초 쉬고 다시 확인 (너무 빨리 돌면 CPU 낭비니까)
                } else {
                    // 진짜 에러인 경우
                    perror("read error");
                    break;
                }
            } else if (nread == 0) {
                // 자식이 연결을 끊음 (EOF)
                printf("[부모] 자식이 연결 끊음. 퇴근.\n");
                break;
            } else {
                // 데이터 읽기 성공!
                printf("[부모] 드디어 받았다!! 내용: %s\n", buf);
                break; // 반복문 탈출
            }
        }
    }
    return 0;
}

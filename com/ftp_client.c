/*ient.c (최소 기능의 PASV FTP 클라이언트)
 * [컴파일] gcc ftp_client.c -o ftp_client
 * [실행] ./ftp_client 127.0.0.1
 * [명령] list | get <filename> | quit
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CONTROL_PORT 2121
#define BUFFER_SIZE 1024

// 서버 응답을 읽고 출력하는 헬퍼 함수
void read_reply(int control_fd) {
    char buffer[BUFFER_SIZE] = {0};
    read(control_fd, buffer, BUFFER_SIZE - 1);
    printf("S -> C: %s", buffer);
}

// [지옥 2] PASV 응답을 파싱하고 2차 데이터 소켓을 "열어서" 반환
int open_data_connection(int control_fd) {
    char buffer[BUFFER_SIZE] = {0};
    int data_fd;
    
    send(control_fd, "PASV\r\n", 6, 0);
    read(control_fd, buffer, BUFFER_SIZE - 1);
    printf("S -> C: %s", buffer);

    if (strncmp(buffer, "227", 3) != 0) {
        printf("PASV 명령 실패!\n");
        return -1;
    }

    // 1. [핵심] 응답 문자열 파싱: "227 ... (h1,h2,h3,h4,p1,p2)"
    int h1, h2, h3, h4, p1, p2;
    char *start = strchr(buffer, '(');
    if (start == NULL) return -1;
    sscanf(start, "(%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2);

    // 2. 포트 번호 계산
    int data_port = p1 * 256 + p2;
    char data_ip[32];
    sprintf(data_ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    printf("PASV: IP: %s, Port: %d\n", data_ip, data_port);

    // 3. 계산된 IP/포트로 2차 데이터 소켓 연결
    data_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in data_addr;
    data_addr.sin_family = AF_INET;
    data_addr.sin_port = htons(data_port);
    inet_pton(AF_INET, data_ip, &data_addr.sin_addr);

    if (connect(data_fd, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
        perror("데이터 포트 연결 실패");
        return -1;
    }
    printf("데이터 포트 연결 성공! (FD: %d)\n", data_fd);
    return data_fd;
}


int main(int argc, char const *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "사용법: %s <Server IP>\n", argv[0]);
        exit(1);
    }

    int control_fd, data_fd;
    struct sockaddr_in serv_addr;
    char user_cmd[512]; // 사용자 입력
    char cmd_buffer[540]; // 서버로 보낼 명령
    char data_buffer[BUFFER_SIZE];
    ssize_t nbytes;
    
    // --- 1. 제어 포트 연결 ---
    control_fd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(CONTROL_PORT);
    inet_pton(AF_INET, argv[1], &serv_addr.sin_addr);

    if (connect(control_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("제어 포트 연결 실패");
        exit(EXIT_FAILURE);
    }

    // --- 2. 로그인 ---
    read_reply(control_fd); // "220 Welcome..."
    send(control_fd, "USER test\r\n", 11, 0);
    read_reply(control_fd); // "331 User OK..."
    send(control_fd, "PASS 1234\r\n", 11, 0);
    read_reply(control_fd); // "230 Logged in..."

	//이미지가 안 보이는 버그로 인해 추가
	printf("S -> C: Switchingto Binary Mode (TYPE I)...\n");
	send(control_fd,"TYPE I\r\n",8,0);
	read_reply(control_fd);

    // --- 3. 사용자 명령 루프 ---
    while (1) {
        printf("\n[ftp> ");
        fgets(user_cmd, 510, stdin);
        user_cmd[strcspn(user_cmd, "\r\n")] = 0; // \n 제거
        
        if (strncmp(user_cmd, "quit", 4) == 0) {
            send(control_fd, "QUIT\r\n", 6, 0);
            read_reply(control_fd);
            break;
        }
        
        // --- [LIST] ---
        else if (strncmp(user_cmd, "list", 4) == 0) {
            // 1. [핵심] 2차 데이터 연결부터 연다
            data_fd = open_data_connection(control_fd);
            if (data_fd < 0) continue;
            
            // 2. 제어 포트로 "LIST" 명령 전송
            send(control_fd, "LIST\r\n", 6, 0);
            
            // 3. 제어 포트로 "150..." 응답 받기
            read_reply(control_fd); 
            
            // 4. [데이터 포트]로 실제 목록 받기
            printf("--- [File List] ---\n");
            while ((nbytes = read(data_fd, data_buffer, BUFFER_SIZE - 1)) > 0) {
                data_buffer[nbytes] = '\0';
                printf("%s", data_buffer);
            }
            printf("-------------------\n");
            close(data_fd); // 데이터 소켓 닫기
            
            // 5. 제어 포트로 "226..." 응답 받기
            read_reply(control_fd);
        }
        
        // --- [GET] ---
        else if (strncmp(user_cmd, "get ", 4) == 0) {
            // 1. [핵심] 2차 데이터 연결부터 연다
            data_fd = open_data_connection(control_fd);
            if (data_fd < 0) continue;

            // 2. 제어 포트로 "RETR filename" 명령 전송
            sprintf(cmd_buffer, "RETR %s\r\n", user_cmd + 4); // "get " 다음부터
            send(control_fd, cmd_buffer, strlen(cmd_buffer), 0);
            
            // 3. 제어 포트로 "150..." 응답 받기
            read_reply(control_fd); 
            
            // 4. [데이터 포트]로 실제 파일 받아서 저장
            FILE* file = fopen(user_cmd + 4, "wb"); // "wb" (바이너리)
            if (file == NULL) {
                perror("파일 생성 실패");
                close(data_fd);
                read_reply(control_fd); // 226 응답 마저 읽기
                continue;
            }
            while ((nbytes = read(data_fd, data_buffer, BUFFER_SIZE)) > 0) {
                fwrite(data_buffer, 1, nbytes, file);
            }
            fclose(file);
            printf("'%s' 다운로드 완료.\n", user_cmd + 4);
            close(data_fd); // 데이터 소켓 닫기
            
            // 5. 제어 포트로 "226..." 응답 받기
            read_reply(control_fd);
        }
        else {
            printf("Unknown command: list | get <filename> | quit\n");
        }
    }

    close(control_fd);
    return 0;
}

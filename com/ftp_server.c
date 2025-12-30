/*erver.c (최소 기능의 PASV FTP 서버)
 * [컴파일] gcc ftp_server.c -o ftp_server
 * [실행] ./ftp_server
 * (주의: 2121번 포트와 50000번대 포트를 사용합니다)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h> // 임시 포트용

#define CONTROL_PORT 2121 // (21번은 root 권한 필요해서 2121로)
#define BUFFER_SIZE 1024

// 클라이언트 1명만 처리하는 함수
void handle_client(int control_fd) {
    char buffer[BUFFER_SIZE];
    char cmd[16], arg[256];
    int data_listen_fd = 0; // [핵심] 데이터 포트 리슨 소켓
    int data_port = 0;

    send(control_fd, "220 Welcome to Minimal FTP Server.\r\n", 36, 0);

    while (read(control_fd, buffer, BUFFER_SIZE - 1) > 0) {
        // \r\n 제거
        buffer[strcspn(buffer, "\r\n")] = 0;
        printf("C -> S: %s\n", buffer);

        sscanf(buffer, "%s %s", cmd, arg);

        // --- 1. 로그인 (가짜) ---
        if (strncmp(cmd, "USER", 4) == 0) {
            send(control_fd, "331 User OK. Need password.\r\n", 29, 0);
        }
        else if (strncmp(cmd, "PASS", 4) == 0) {
            send(control_fd, "230 User logged in.\r\n", 23, 0);
        }
	
	//이미지 안 보이는 버그로 인해 추가
	else if (strncmp(cmd, "TYPE",4)==0){
		if (strcmp(arg,"I")==0){
			send(control_fd, "200 Type set to I.\r\n",22,0);
		}
		else if (strcmp(arg,"A")==0){
			send(control_fd,"200 Type set to A.\r\n",22,0);
		}
		else {
			send(control_fd, "504 Unknown TYPE.\r\n",21,0);
		}
	}
        // --- 2. [지옥 1] PASV (패시브 모드) ---
        // 클라이언트가 "데이터 포트 열어줘"라고 요청
        else if (strncmp(cmd, "PASV", 4) == 0) {
            // (이전에 연 데이터 리슨 소켓이 있다면 닫음)
            if (data_listen_fd > 0) close(data_listen_fd);

            // 1. 새로운 데이터 리슨 소켓 생성
            data_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
            
            // 2. 임시 포트에 바인드 (50000 ~ 51000 사이)
            srand(time(NULL));
            data_port = 50000 + (rand() % 1000);
           
            struct sockaddr_in data_addr;
            data_addr.sin_family = AF_INET;
            data_addr.sin_addr.s_addr = INADDR_ANY;
            data_addr.sin_port = htons(data_port);
            
            if (bind(data_listen_fd, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
                perror("PASV bind failed");
                continue;
            }
            listen(data_listen_fd, 1);
            
            // 3. [핵심] IP와 포트 번호를 클라이언트에게 "계산"해서 알려줌
            // p1 = data_port / 256
            // p2 = data_port % 256
            // (IP는 127.0.0.1로 하드코딩)
            int p1 = data_port / 256;
            int p2 = data_port % 256;
            char pasv_reply[256];
            sprintf(pasv_reply, "227 Entering Passive Mode (127,0,0,1,%d,%d)\r\n", p1, p2);
            printf("S -> C: %s", pasv_reply);
            send(control_fd, pasv_reply, strlen(pasv_reply), 0);
        }

        // --- 3. LIST (파일 목록) ---
        else if (strncmp(cmd, "LIST", 4) == 0) {
            // 1. PASV가 미리 열어둔 "데이터 리슨 소켓"으로 2차 연결 수락
            int data_fd = accept(data_listen_fd, NULL, NULL);
            if (data_fd < 0) {
                perror("LIST accept failed");
                continue;
            }
            // 2. 제어 포트로 "이제 데이터 보낼게" 신호
            send(control_fd, "150 Here comes the directory listing.\r\n", 39, 0);

            // 3. [데이터 소켓]으로 ls -l 결과 전송
            FILE* pipe = popen("ls -l", "r");
            while (fgets(buffer, BUFFER_SIZE, pipe) != NULL) {
                send(data_fd, buffer, strlen(buffer), 0);
            }
            pclose(pipe);

            // 4. [데이터 소켓] 닫기 (전송 끝!)
            close(data_fd);
            close(data_listen_fd);
            data_listen_fd = 0; // 리슨 소켓 리셋
            
            // 5. 제어 포트로 "전송 끝났어" 신호
            send(control_fd, "226 Directory send OK.\r\n", 26, 0);
        }
        
        // --- 4. RETR (다운로드) ---
        else if (strncmp(cmd, "RETR", 4) == 0) {
            // (LIST와 거의 동일. popen 대신 fopen)
            int data_fd = accept(data_listen_fd, NULL, NULL);
            if (data_fd < 0) continue;
            send(control_fd, "150 Opening data connection for file.\r\n", 39, 0);

            FILE* file = fopen(arg, "rb"); // "rb" (바이너리)
            if (file == NULL) {
                send(control_fd, "550 File not found.\r\n", 21, 0);
            } else {
                ssize_t nbytes;
                while ((nbytes = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                    if(send(data_fd, buffer, nbytes, 0) < 0)break;
                }
                fclose(file);
                send(control_fd, "226 Transfer complete.\r\n", 24, 0);
            }
            close(data_fd);
            close(data_listen_fd);
            data_listen_fd = 0;
        }

        // --- 5. QUIT (종료) ---
        else if (strncmp(cmd, "QUIT", 4) == 0) {
            send(control_fd, "221 Goodbye.\r\n", 14, 0);
            break;
        }
        else {
            send(control_fd, "500 Unknown command.\r\n", 22, 0);
        }
    }
    printf("클라이언트 연결 종료됨.\n");
    if (data_listen_fd > 0) close(data_listen_fd);
}


// --- 메인 함수 (단순히 accept만) ---
int main() {
    int server_fd, control_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int opt = 1;
    
    // (서버 기본 설정: socket, setsockopt, bind, listen)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(CONTROL_PORT);
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 5);

    printf("FTP 서버가 %d번 포트에서 기다리는 중...\n", CONTROL_PORT);

    while (1) {
        control_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
        if (control_fd < 0) {
            perror("accept failed");
            continue;
        }
        printf("클라이언트 연결됨! (제어 포트)\n");
        
        // (원래는 여기서 fork() 로 다중 접속 처리해야 함)
        handle_client(control_fd); // 단순화를 위해 1명씩만 처리
        
        close(control_fd);
    }

    close(server_fd);
    return 0;
}

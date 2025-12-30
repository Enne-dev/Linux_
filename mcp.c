#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <errno.h>

void copy_file(const char *source, const char *destination) {
    int src_fd, dest_fd;
    char buffer[4096];
    ssize_t bytes_read;

    src_fd = open(source, O_RDONLY);
    if (src_fd == -1) {
        perror("Error opening source file");
	exit(1);
	}

	
	 dest_fd = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd == -1) {
        perror("Error opening destination file");
        close(src_fd);
        exit(1);
    }

 while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        if (write(dest_fd, buffer, bytes_read) != bytes_read) {
            perror("Error writing to destination file");
            close(src_fd);
            close(dest_fd);
            exit(1);
        }
    }

    close(src_fd);
    close(dest_fd);
}
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "사용법: %s <원본파일> <대상파일>\n", argv[0]);
        exit(1);
    }

    char *source_file = argv[1];
    char *dest_file = argv[2];
    char backup_dir[] = ".versions";
    char backup_path[256];

    if (access(dest_file, F_OK) == 0) {
	printf("'%s' 파일이 이미 존재합니다. 원본을 백업합니다...\n", dest_file);
if (mkdir(backup_dir, 0755) == -1) {
        if (errno != EEXIST) {
            perror("백업 디렉터리 생성 실패");
            exit(1);
        }
    }

    sprintf(backup_path, "%s/%s.%ld", backup_dir, dest_file, time(NULL));

    if (rename(dest_file, backup_path) == -1) {
        perror("백업 파일로 이동 실패");
        exit(1);
    }
    printf("백업 완료: %s\n", backup_path);
}

printf("'%s' -> '%s' 복사 중...\n", source_file, dest_file);
copy_file(source_file, dest_file);
printf("복사 완료.\n");

return 0;
}

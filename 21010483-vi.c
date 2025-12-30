#include <unistd.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[]){
	int fd;
	char buffer[1024];
	ssize_t bytes_read;

	if (access(argv[1], F_OK) == -1) {
		fd = open(argv[1], O_WRONLY | O_CREAT, 0600);
	}
	else {
		fd = open(argv[1], O_RDWR | O_APPEND);
		lseek(fd,0,SEEK_SET);

		while ((bytes_read = read(fd, buffer, 1024)) > 0) {
			write(1, buffer, bytes_read);
		}
	}

	while (1) {
		bytes_read = read(0, buffer, 1024);
		if (bytes_read <= 0) break;
		if (bytes_read == 5 && strncmp(buffer, "quit", 4) == 0) break;
		write(fd, buffer, bytes_read);
	}

	close(fd);

	return 0;
}	

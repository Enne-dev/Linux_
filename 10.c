#include<fcntl.h>
#include<unistd.h>
#include<stdio.h>
#include<string.h>
#include<dirent.h>
#include<ftw.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>

int main(void){
        char in[100], *res[20]={0};
        int i, status;
	pid_t pid;

        while (1){
		getcwd(in, sizeof(in));
	
		i=0;
        	res[i]=strtok(in, "/");
		while (res[i]){
			res[++i]=strtok(NULL, "/");
		}
               	printf("%s > ", res[i-1]);

        	fgets(in, sizeof(in), stdin);
		in[strcspn(in, "\n")]='\0';
		if (in[0]=='\0')
			continue;

		i=0;
        	res[i]=strtok(in, " ");
		while (res[i]){
			res[++i]=strtok(NULL, " ");
		}

		if (res[0] == NULL)  continue;
		if (strcmp(res[0], "exit") == 0) exit(0);
		if (strcmp(res[0], "cd") == 0) {
			chdir(res[1]);
			continue;
		}

		pid = fork();
		if (pid == 0) {
			execvp(res[0], res);
			exit(1);
		}
		else wait(&status);

	}

	return 0;
}

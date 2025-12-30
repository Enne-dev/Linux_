#include "c.h"

void cd_m(char **res){
	chdir(res[1]);
}

void mkdir_m(char **res){
	mkdir(res[1],0700);
}

void ls_m(char **res){
	struct dirent *d;
	DIR *dp;

	dp=opendir(".");
	while (d=readdir(dp)){
		if (d->d_name[0] != '.'){
			printf("%s\n",d->d_name);
		}
	}
	closedir(dp);
}
//위에 코드 전부 다시 보기

int main(void){
	char in[100], *res[20]={0};
	char *inst[3]={"cd", "mkdir", "ls"};
	void (*f[3])(char**)={cd_m, mkdir_m, ls_m};
	int i;

	while(1){
		getcwd(in,99);
		printf("%s> ",in);
		//위에 두 줄 다시 보기 

		if (fgets(in, sizeof(in), stdin) == NULL || strlen(in) == 1)
			continue;
		size_t len = strlen(in);
		if (in[len-1] == '\n') in[len-1] = '\0';

		i=0;
		res[i]=strtok(in, " ");
		while(res[i]&&i<19){
			i++;
			res[i] = strtok(NULL, " ");
		}
		if (res[0] == NULL) continue;
		if(!strcmp(res[0],"exit"))
			exit(0);
		for(i=0;i<3;i++){
			if(!strcmp(res[0],inst[i])){
				f[i](res);
				break;
			}
		}
	}
	return 0;
}

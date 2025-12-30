#include "c.h"

int main(int argc, char** argv) {
	int i,j;

	for (i=0;i<3;i++) {
		for (j=1;j<argc;j++) {
			printf("%s",argv[j]);
		}
		printf("\n");
	}

	exit(0);
}

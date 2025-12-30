#include "c.h"

int main(void) {
	
	char ch1[101], ch2[101];
	scanf("%s%s",ch1,ch2);
	symlink(ch1,ch2);
	
	return 0;
}

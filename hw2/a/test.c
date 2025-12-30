#include <stdio.h>
#include "myio4.h"

int main(void) {
	char ch;
	int N,i,num,sum=0,cnt=0;
	float f[20], max=0;

	r_init();

	r_scanf("%d",&N);

	for(i=0;i<N;i++){
		r_scanf("%d",&num);
		sum += num;
		r_printf("sum=%d\n",sum);
	}

	r_scanf("%d",&N);
	for(i=0;i<N;i++){
		r_scanf("%f",&f[i]);
	}
	
	for(i=0;i<N;i++){
		if (f[i]>max) max = f[i];
		r_printf("max=%.2f\n",max);
	}
	r_scanf("%d",&N);
	for (i=0;i<N;i++){
		r_scanf(" %c",&ch);
		if (ch>='a' && ch <= 'z') cnt++;
		r_printf("cnt=%d\n",cnt);
	}
	
	r_cleanup();

	return 0;
}


#include "c.h"

struct q_entry {
	long mtype;
	int data;
};

int main(int argc, char** argv){
	int i,qid,in,id;
	key_t key;
	struct q_entry msg;
	
	id = atoi(argv[1]);
	
	key = ftok("key", 3);

	qid = msgget(key, IPC_CREAT|0600);
	
	for (i=0;i<5;i++){
		scanf("%d",&in);
		msg.mtype = id;
		msg.data = in;
		msgsnd(qid, &msg, sizeof(int), 0);
		msgrcv(qid, &msg, sizeof(int), id+3, 0);
		printf("%d\n", msg.data);
	}	
	return 0;
}

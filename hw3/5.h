#ifndef 5_H
#define 5_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/ipc.h>
#include<sys/msg.h>

struct MsgBuf{
	long mtype;//메세지 수신 대상 ID
	int sender_id;
	int seq_num;
	char text[512];
};


void sender(int qid, int my_id){
	struct MsgBuf msg;


		
	while(1){
		






void receiver(int qid, int my_id){
	struct MsgBuf msg;
	int len;

	while(1){
		len=msgrcv(qid,&msg,sizeof(struct MsgBuf)-sizeof(long), my_id, 0);

		if (strcmp(msg.text, "talk_quit")==0) break;

		if (msg.sender_id != my_id){
			printf("[sender=%d &msg#=%d] %s\n",msg.sender_id,msg.seq_num,msg.text);
		}
	}

	exit(0);
}





#endif

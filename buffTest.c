#include<stdio.h>
extern int writerSetBuffer(int shareID, char** buffer);
extern int readerGetBuffer(int shareID, char** buffer);
extern unsigned int writeBuffer(char* circleBuff, char* data,unsigned int length);
extern int readBuffer(char* circleBuff, char* data,int datalen);
extern int clearBuffer(char* circleBuff);


//for writer
int main()
{
	char *buff[4];
	int buffID[4]={8000,8001,8002,8003};
	
	char data[10000];
	writerSetBuffer(8001, &buff[0]);
	writerSetBuffer(8002, &buff[1]);
	writerSetBuffer(8003, &buff[2]);
	writerSetBuffer(8004, &buff[3]);
	sleep(3);
	while(1){
		writeBuffer(buff[0],data,10000);
		writeBuffer(buff[1],data,10000);
		writeBuffer(buff[2],data,10000);
		writeBuffer(buff[3],data,10000);
		usleep(1000);
	}	
		
}/*
//for reader
int main()
{
	char *buff[4];
	int buffID[4]={8000,8001,8002,8003};
	
	char data[10000];
	readerGetBuffer(8001, &buff[0]);
	readerGetBuffer(8002, &buff[1]);
	readerGetBuffer(8003, &buff[2]);
	readerGetBuffer(8004, &buff[3]);
	sleep(3);
	clearBuffer(buff[0]);
	clearBuffer(buff[1]);
	clearBuffer(buff[2]);
	clearBuffer(buff[3]);

	while(1){
		readBuffer(buff[0],data,10000);
		readBuffer(buff[1],data,10000);
		readBuffer(buff[2],data,10000);
		readBuffer(buff[3],data,10000);
		usleep(1000);
	}	
		
}*/

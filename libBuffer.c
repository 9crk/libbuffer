#include <sys/shm.h>  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>
/*
extern int writerSetBuffer(int shareID, char** buffer);
extern int readerGetBuffer(int shareID, char** buffer);
extern unsigned int writeBuffer(char* circleBuff, char* data,unsigned int length);
extern int readBuffer(char* circleBuff, char* data,int datalen);
extern int clearBuffer(char* circleBuff);
extern int readBufferEx(char* circleBuff, char* data,int datalen);
//if i didnt got datalen size of data, i just return 0, do no read.
*/
#define BUFFER_LEN  (1024*1024 - 12)
#define SHARED_BUFFER_LEN (BUFFER_LEN + 12)
#define TRUE 1
#define FALSE 0
#pragma pack(push) //保存对齐状态
#pragma pack(4)//设定为4字节对齐
 typedef struct {
	unsigned int readIndex;
	unsigned int writeIndex;
	unsigned int allowWrite;
	char buffer[BUFFER_LEN];
}circleBuffer;
#pragma pack(pop) 

void* membuff;


//返回共享内存的fd， 输入ID和buffer指针，创建内存
int writerSetBuffer(int shareID, char** buffer)
{
	int shmid;
	circleBuffer **buff = (circleBuffer**)buffer; 
	shmid =shmget( shareID,SHARED_BUFFER_LEN,0666|IPC_CREAT );    
	if(shmid >= 0){
		printf( "Create a shared memory segment %d\n", shmid);  
	}
	membuff = shmat( shmid, ( const void* )0,0 );  
	if(membuff == (void*)-1){
		printf("shmat error: %s!\n", strerror(errno));
		return -1;
	}
	*buff = (circleBuffer*)membuff;
	(*buff)->allowWrite = 1;
	(*buff)->readIndex = 0;
	(*buff)->writeIndex = 0;
	return shmid;
}
int readerGetBuffer(int shareID, char** buffer)
{
	int shmid;
	circleBuffer **buff = (circleBuffer**)buffer; 
	shmid =shmget( shareID,0,0666);    
	if( shmid >= 0 ) { 
		printf( "Create a shared memory segment %d\n", shmid);  
	}
	membuff = shmat( shmid, (const void*)0,0 );  
	if(membuff == (void*)-1){
		printf("shmat error: %s!\n", strerror(errno));
	}
	*buff = (circleBuffer*)membuff;
	return shmid;
}
/*
返回：
0  读不出来了
n  读了n个字节回来
注意：读n个，可能返回n-x个，注意做判断
*/
int readBuffer(char* circleBuff, char* data,int datalen)
{
	circleBuffer* circleBuf =(circleBuffer*)circleBuff; 
	unsigned int writeIndex = circleBuf->writeIndex;
	unsigned int readIndex  = circleBuf->readIndex;
	int ret;
	//printf("WriteIndex is %d readIndex is %d Del=%d\n",writeIndex, readIndex,datalen);
	if (readIndex < writeIndex){								
		if (readIndex + datalen < writeIndex){					//------------r---l---w-------------
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													//------------r------w----l--------------
			memcpy(data, circleBuf->buffer + readIndex, writeIndex - readIndex);
			circleBuf->readIndex = writeIndex;
			circleBuf->allowWrite = TRUE;
			printf("no data...\n");
			ret = writeIndex - readIndex;
		}
	}else if (readIndex > writeIndex){
		if (readIndex + datalen < BUFFER_LEN){					//----w----------------------r-----l----
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													
			if (readIndex + datalen - BUFFER_LEN < writeIndex){//---l----w------------------------r----
				memcpy(data, circleBuf->buffer + readIndex, BUFFER_LEN - readIndex);
				memcpy(data + BUFFER_LEN - readIndex, circleBuf->buffer, readIndex + datalen - BUFFER_LEN);		
				circleBuf->readIndex = readIndex + datalen - BUFFER_LEN;
				ret = datalen;
			}else{											  //--w----l--------------------------r---
				printf("no data...\n");			
				circleBuf->allowWrite = TRUE;
				memcpy(data, circleBuf->buffer + readIndex, BUFFER_LEN - readIndex);
				memcpy(data + BUFFER_LEN - readIndex, circleBuf->buffer, writeIndex);
				circleBuf->readIndex = writeIndex;
				ret = writeIndex + BUFFER_LEN - readIndex;
			}
		}
	}else{													//-----------w==r--------------------------
		circleBuf->allowWrite = TRUE;
		ret = 0;
	}
	return ret;
}
int clearBuffer(char* circleBuff)
{
	int i;
	int found = 0;
	circleBuffer* circleBuf =(circleBuffer*)circleBuff;
	if(circleBuf->writeIndex < circleBuf->readIndex){ // --------w------------r------
		for(i = circleBuf->writeIndex; i>=3; i--){
			if(circleBuf->buffer[i] == 0x00 && circleBuf->buffer[i-1] == 0x00 && circleBuf->buffer[i-2] == 0x00 && circleBuf->buffer[i-3] == 0x01){
				circleBuf->readIndex = i-3;
				found = 1;
				break;
			}
		}
		if(!found){
			for(i = circleBuf->readIndex; i< (BUFFER_LEN-4); i++){
				if(circleBuf->buffer[i] == 0x00 && circleBuf->buffer[i+1] == 0x00 && circleBuf->buffer[i+2] == 0x00 && circleBuf->buffer[i+3] == 0x01){
					circleBuf->readIndex = i;
					found = 1;
					break;
				}
			}
		}
		return 1;//找到了
	}else{											//---------r---------------w-----
		if(circleBuf->readIndex <= circleBuf->writeIndex -4){
			for(i = circleBuf->readIndex; i< (circleBuf->writeIndex-4); i++){
				if(circleBuf->buffer[i] == 0x00 && circleBuf->buffer[i+1] == 0x00 && circleBuf->buffer[i+2] == 0x00 && circleBuf->buffer[i+3] == 0x01){
					circleBuf->readIndex = i;
					found = 1;
					break;
				}
			}
		}
	}
	if(!found){
		circleBuf->readIndex = circleBuf->writeIndex;
		return 0;
	}
	return 1;
}
int readBufferEx(char* circleBuff, char* data,int datalen)
{
	circleBuffer* circleBuf =(circleBuffer*)circleBuff; 
	unsigned int writeIndex = circleBuf->writeIndex;
	unsigned int readIndex  = circleBuf->readIndex;
	int ret;
	//printf("WriteIndex is %d readIndex is %d Del=%d\n",writeIndex, readIndex,datalen);
	if (readIndex < writeIndex){								
		if (readIndex + datalen < writeIndex){					//------------r---l---w-------------
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													//------------r------w----l--------------
			//memcpy(data, circleBuf->buffer + readIndex, writeIndex - readIndex);
			//circleBuf->readIndex = writeIndex;
			//circleBuf->allowWrite = TRUE;
			printf("no data...\n");
			ret = 0;//writeIndex - readIndex;
		}
	}else if (readIndex > writeIndex){
		if (readIndex + datalen < BUFFER_LEN){					//----w----------------------r-----l----
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													
			if (readIndex + datalen - BUFFER_LEN < writeIndex){//---l----w------------------------r----
				memcpy(data, circleBuf->buffer + readIndex, BUFFER_LEN - readIndex);
				memcpy(data + BUFFER_LEN - readIndex, circleBuf->buffer, readIndex + datalen - BUFFER_LEN);		
				circleBuf->readIndex = readIndex + datalen - BUFFER_LEN;
				ret = datalen;
			}else{											  //--w----l--------------------------r---
				printf("no data...\n");			
				//circleBuf->allowWrite = TRUE;
				//memcpy(data, circleBuf->buffer + readIndex, BUFFER_LEN - readIndex);
				//memcpy(data + BUFFER_LEN - readIndex, circleBuf->buffer, writeIndex);
				//circleBuf->readIndex = writeIndex;
				ret = 0;//writeIndex + BUFFER_LEN - readIndex;
			}
		}
	}else{													//-----------w==r--------------------------
		circleBuf->allowWrite = TRUE;
		ret = 0;
	}
	return ret;
}
/*
int readBufferEx(circleBuffer* circleBuf, char* data)
{
	unsigned int writeIndex = circleBuf->writeIndex;
	unsigned int readIndex  = circleBuf->readIndex;
	int ret;
	//printf("WriteIndex is %d readIndex is %d Del=%d\n",writeIndex, readIndex,datalen);
	if (readIndex < writeIndex){								
		if (readIndex + datalen < writeIndex){					//------------r---l---w-------------
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													//------------r------w----l--------------
			memcpy(data, circleBuf->buffer + readIndex, writeIndex - readIndex);
			circleBuf->readIndex = writeIndex;
			circleBuf->allowWrite = TRUE;
			printf("no data...\n");
			ret = writeIndex - readIndex;
		}
	}else if (readIndex > writeIndex){
		if (readIndex + datalen < BUFFER_LEN){					//----w----------------------r-----l----
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													
			if (readIndex + datalen - BUFFER_LEN < writeIndex){//---l----w------------------------r----
				memcpy(data, circleBuf->buffer + readIndex, BUFFER_LEN - readIndex);
				memcpy(data + BUFFER_LEN - readIndex, circleBuf->buffer, readIndex + datalen - BUFFER_LEN);		
				circleBuf->readIndex = readIndex + datalen - BUFFER_LEN;
				ret = datalen;
			}else{											  //--w----l--------------------------r---
				printf("no data...\n");			
				circleBuf->allowWrite = TRUE;
				memcpy(data, circleBuf->buffer + readIndex, BUFFER_LEN - readIndex);
				memcpy(data + BUFFER_LEN - readIndex, circleBuf->buffer, writeIndex);
				circleBuf->readIndex = writeIndex;
				ret = writeIndex + BUFFER_LEN - readIndex;
			}
		}
	}else{													//-----------w==r--------------------------
		circleBuf->allowWrite = TRUE;
		ret = 0;
	}
	return ret;
}*/
/*
返回：
0   不允许写，r+l > w
0   不允许写，r = w
n   写入了n个字节的数据
*/
//返回是否成功              被存储的buffer       数据          长度
unsigned int writeBuffer(char* circleBuff, char* data,unsigned int length){
	circleBuffer* circleBuf =(circleBuffer*)circleBuff; 
	unsigned int writeIndex = circleBuf->writeIndex;
	unsigned int readIndex = circleBuf->readIndex;
	
//	printf("w is %d  r is %d Add %d bytes\n",writeIndex,readIndex,length);
	if (writeIndex >= readIndex){
		if (writeIndex == readIndex && circleBuf->allowWrite == FALSE){
			printf("full, r==w\n");
			return 0;
		}
		if ((writeIndex + length) > BUFFER_LEN){				
			if (writeIndex + length - BUFFER_LEN < readIndex){		//----l--r-------------------w--
				memcpy(circleBuf->buffer + writeIndex, data, BUFFER_LEN - writeIndex);
				memcpy(circleBuf->buffer, data + BUFFER_LEN - writeIndex, writeIndex + length - BUFFER_LEN);
				circleBuf->writeIndex = writeIndex + length - BUFFER_LEN;
			}else{													//---r--l---------------------w-
				printf("full, w+l > r\n");
				return 0;
			}
		}else{														//-----r-----------------w----l--
			memcpy(circleBuf->buffer + writeIndex, data, length);
			circleBuf->writeIndex += length;
		}
	}else if (writeIndex < readIndex){								
		if (writeIndex + length >= readIndex){						//------------w--r--l-----------应当丢失，报错，返回0
			printf("full, w+l > r\n");
			return 0;
		}else{														//------------w---l---r----------正常，返回l
			memcpy(circleBuf->buffer + writeIndex, data, length);
			circleBuf->writeIndex += length;
		}
	}
	return length;
}
/*
//reader  test
int main()
{
char* p;
int a=readerGetBuffer(1342,&p);
char data[100000];
int i;
int n;
	while(1)
	{
		n = readBuffer(p,data, 100000);
		if(n>0){
			for(i=0;i<10;i++)printf("%c", data[i]);
			printf("\n");
		}
		printf("I read %d bytes\n", n);
		sleep(1);
	}
}

//writer test
int main()
{
char* p;
int a=writerSetBuffer(1342,&p);
char data[100000];
int i;
	while(1)
	{
		memset(data, 0, 1000);
		scanf("%s", data);
		writeBuffer(p,data, 100000);
		for(i=0;i<10;i++)printf("%c", data[i]);
		printf("\n");
	}
}
*/

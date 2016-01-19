/*****************************************************
 * author:admin@9crk.com all rights reserved! 
 * forbidden copy or any form of distributing
 * **************************************************/
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

#define TRUE 1
#define FALSE 0
typedef struct zfifo{
	unsigned int readIndex;
	unsigned int writeIndex;
	pthread_mutex_t mutex;
	char* buffer;
	unsigned int dataLen;
	int allowWrite;
}zfifo;
/******************************************************
ring buffer
******************************************************/
#define NMAX 32
typedef struct ringbuf {
	unsigned char *buffer;
	int frame_type;//0 ,I frame, 1 p frame    
	int size;
	unsigned long long pts;
}ringbuf;
typedef struct ringfifo{
	int fput; /* 环形缓冲区的当前放入位置 */
	int fget; /* 缓冲区的当前取出位置 */
	int fcount; /* 环形缓冲区中的元素总数量 */
	int fpts;/* 最新一帧的pts*/
//	int pureVideoCount;
//	int pureAudioCount;
	ringbuf buffer[NMAX];
	pthread_mutex_t fmutex;
}ringfifo;

extern int ringfifo_init(char** handle, unsigned int len);
extern int ringfifo_clear(char* handle);
extern int ringfifo_destroy(char* handle);
extern int ringfifo_read(char* handle, char** pData, unsigned int *datalen, int *type, unsigned long long *pts);
extern int ringfifo_write(char* handle, char* data, unsigned int length, int encode_type,unsigned long long pts);

int ringfifo_init(char** handle, unsigned int len)
{
	int i;
	ringfifo *p = (ringfifo*)malloc(sizeof(ringfifo));
	*handle = (char*)p;
	if(p == NULL)return -1;
	p->fput = 0; /* 环形缓冲区的当前放入位置 */
	p->fget = 0; /* 缓冲区的当前取出位置 */
	p->fcount = 0; /* 环形缓冲区中的元素总数量 */
	
    for(i =0; i<NMAX; i++)
    {
        p->buffer[i].buffer = (unsigned char*)malloc(len);
        p->buffer[i].size = 0;
        p->buffer[i].frame_type = 0;
       // printf("FIFO INFO:idx:%d,len:%d,ptr:%x\n",i,ringfifo[i].size,(int)(ringfifo[i].buffer));
    }
	if(pthread_mutex_init(&p->fmutex,NULL) != 0 )  
    {  
	   printf("Init mutex %d error.",i);  
	   return -1;
    }  
	return 0;
}
int ringfifo_clear(char* handle)
{
	ringfifo*p = (ringfifo*)handle;
	p->fput = 0; /* 环形缓冲区的当前放入位置 */
    p->fget = 0; /* 缓冲区的当前取出位置 */
    p->fcount = 0; /* 环形缓冲区中的元素总数量 */
}
int ringfifo_destroy(char* handle)
{
	int i;
	ringfifo *p = (ringfifo*)handle;
    printf("begin free mem\n");
    for(i =0; i<NMAX; i++)
    {
       // printf("FREE FIFO INFO:idx:%d,len:%d,ptr:%x\n",i,ringfifo[i].size,(int)(ringfifo[i].buffer));
        free(p->buffer[i].buffer);
        p->buffer[i].size = 0;
    }
	free(handle);
	pthread_mutex_destroy(&p->fmutex);
}
int ringfifo_read(char* handle, char** pData, unsigned int *datalen, int *type, unsigned long long *pts)
{
	int Pos;
	int tmp;
	ringfifo*p = (ringfifo*)handle;
	pthread_mutex_lock(&p->fmutex);
    if(p->fcount > 0)
    {
		//printf("fget is %d fput is %d\n",fget,fput);
        Pos = p->fget;
		p->fget = ((p->fget +1) == NMAX ? 0 : (p->fget + 1));
        p->fcount--;
       	*pData = p->buffer[Pos].buffer;
        *type = p->buffer[Pos].frame_type;
        *datalen =	p->buffer[Pos].size;
		*pts =	p->buffer[Pos].pts;
        //printf("Get FIFO INFO:idx:%d,len:%d,ptr:%x,type:%d\n",Pos,getinfo->size,(int)(getinfo->buffer),getinfo->frame_type);
		tmp = ((p->fget +1) == NMAX ? 0 : (p->fget + 1));
		p->fpts = p->buffer[tmp].pts;
        pthread_mutex_unlock(&p->fmutex);
        return p->buffer[Pos].size;
    }
    else
    {
        //printf("Buffer is empty\n");
        pthread_mutex_unlock(&p->fmutex);
        return 0;
    }
}
int ringfifo_write(char* handle, char* data, unsigned int length, int encode_type,unsigned long long pts)
{
	ringfifo* p = (ringfifo*)handle;
	pthread_mutex_lock(&p->fmutex);
    if(p->fcount < (NMAX-2))
    {
        memcpy(p->buffer[p->fput].buffer,data,length);
        p->buffer[p->fput].size= length;
        p->buffer[p->fput].frame_type = encode_type;
		p->buffer[p->fput].pts = pts;
        //printf("Put FIFO INFO:idx:%d,len:%d,ptr:%x,type:%d\n",iput,ringfifo[iput].size,(int)(ringfifo[iput].buffer),ringfifo[iput].frame_type);
        p->fput = ((p->fput +1) == NMAX ? 0 : (p->fput + 1));
        p->fcount++;
    }
    else
    {
          //printf("Buffer is full\n");
    }
	p->fpts = pts;
	pthread_mutex_unlock(&p->fmutex);
}

/*****************************************************/
extern int zfifo_init(char** handle, unsigned int len);
extern int zfifo_destroy(char* handle);
extern int zfifo_read(char* handle, char* data, unsigned int datalen);
extern int zfifo_readEx(char* handle, char* data, unsigned int datalen);
extern int zfifo_write(char* handle, char* data, unsigned int length);
extern int zfifo_check(char *handle);
extern int zfifo_clear(char* handle);
extern int zfifo_clearA(char* handle);

int zfifo_init(char** handle, unsigned int len)
{
		zfifo *p = malloc(sizeof(zfifo));
		if(p==NULL){printf("TaiHenDa!!\n");return -1;}
		memset((void*)p, 0, sizeof(zfifo));
		p->buffer = (char*)malloc(len);
		if(p->buffer==NULL){printf("TaiHenDa!!\n");return -1;}
		p->dataLen = len;
		p->allowWrite = TRUE;
		if(pthread_mutex_init(&p->mutex,NULL) != 0 )  
	    {  
		   printf("Init mutex error.\n");  
		   return -1;
	    }  
		*handle = (char*)p;
		return 0;
}
int zfifo_destroy(char* handle)
{
	pthread_mutex_destroy(&((zfifo*)handle)->mutex);
	free(((zfifo*)handle)->buffer);
	free(handle);
}
int zfifo_read(char* handle, char* data, unsigned int datalen)
{
	int readIndex,writeIndex;
	int ret;
	zfifo* circleBuf = (zfifo*)handle;
	pthread_mutex_lock(&circleBuf->mutex);
	
	readIndex = circleBuf->readIndex;
	writeIndex = circleBuf->writeIndex;
	
	if (readIndex < writeIndex){								
		if (readIndex + datalen < writeIndex){					//------------r---l---w-------------
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													//------------r------w----l--------------
			memcpy(data, circleBuf->buffer + readIndex, writeIndex - readIndex);
			circleBuf->readIndex = writeIndex;
			circleBuf->allowWrite = TRUE;
			//printf("no data...\n");
			ret = writeIndex - readIndex;
		}
	}else if (readIndex > writeIndex){
		if (readIndex + datalen < circleBuf->dataLen){					//----w----------------------r-----l----
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													
			if (readIndex + datalen - circleBuf->dataLen < writeIndex){//---l----w------------------------r----
				memcpy(data, circleBuf->buffer + readIndex, circleBuf->dataLen - readIndex);
				memcpy(data + circleBuf->dataLen - readIndex, circleBuf->buffer, readIndex + datalen - circleBuf->dataLen);		
				circleBuf->readIndex = readIndex + datalen - circleBuf->dataLen;
				ret = datalen;
			}else{											  //--w----l--------------------------r---
				//printf("no data...\n");			
				circleBuf->allowWrite = TRUE;
				memcpy(data, circleBuf->buffer + readIndex, circleBuf->dataLen - readIndex);
				memcpy(data + circleBuf->dataLen - readIndex, circleBuf->buffer, writeIndex);
				circleBuf->readIndex = writeIndex;
				ret = writeIndex + circleBuf->dataLen - readIndex;
			}
		}
	}else{													//-----------w==r--------------------------
		circleBuf->allowWrite = TRUE;
		ret = 0;
	}
	pthread_mutex_unlock(&circleBuf->mutex);
	return ret;
}
int zfifo_readEx(char* handle, char* data, unsigned int datalen)
{
	int readIndex,writeIndex;
	int ret;
	zfifo* circleBuf = (zfifo*)handle;
	pthread_mutex_lock(&circleBuf->mutex);
	
	readIndex = circleBuf->readIndex;
	writeIndex = circleBuf->writeIndex;
	
	if (readIndex < writeIndex){								
		if (readIndex + datalen < writeIndex){					//------------r---l---w-------------
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													//------------r------w----l--------------
			//memcpy(data, circleBuf->buffer + readIndex, writeIndex - readIndex);
			//circleBuf->readIndex = writeIndex;
			//circleBuf->allowWrite = TRUE;
			//printf("no data...\n");
			ret = 0;//writeIndex - readIndex;
		}
	}else if (readIndex > writeIndex){
		if (readIndex + datalen < circleBuf->dataLen){					//----w----------------------r-----l----
			memcpy(data, circleBuf->buffer + readIndex, datalen);
			circleBuf->readIndex += datalen;
			ret = datalen;
		}else{													
			if (readIndex + datalen - circleBuf->dataLen < writeIndex){//---l----w------------------------r----
				memcpy(data, circleBuf->buffer + readIndex, circleBuf->dataLen - readIndex);
				memcpy(data + circleBuf->dataLen - readIndex, circleBuf->buffer, readIndex + datalen - circleBuf->dataLen);		
				circleBuf->readIndex = readIndex + datalen - circleBuf->dataLen;
				ret = datalen;
			}else{											  //--w----l--------------------------r---
				//printf("no data...\n");			
				//circleBuf->allowWrite = TRUE;
				//memcpy(data, circleBuf->buffer + readIndex, circleBuf->dataLen - readIndex);
				//memcpy(data + circleBuf->dataLen - readIndex, circleBuf->buffer, writeIndex);
				//circleBuf->readIndex = writeIndex;
				ret = 0;//writeIndex + circleBuf->dataLen - readIndex;
			}
		}
	}else{													//-----------w==r--------------------------
		circleBuf->allowWrite = TRUE;
		ret = 0;
	}
	pthread_mutex_unlock(&circleBuf->mutex);
	return ret;
}
int zfifo_write(char* handle, char* data, unsigned int length)
{
	int writeIndex,readIndex;
	int ret=0;
	zfifo* circleBuf = (zfifo*)handle;
	pthread_mutex_lock(&circleBuf->mutex);
	
	writeIndex  = circleBuf->writeIndex;
	readIndex = circleBuf->readIndex;
	
	if (writeIndex >= readIndex){
		if (writeIndex == readIndex && circleBuf->allowWrite == FALSE){
			//printf("full, r==w\n");
			ret = 0;
		}
		if ((writeIndex + length) > circleBuf->dataLen){				
			if (writeIndex + length - circleBuf->dataLen < readIndex){		//----l--r-------------------w--
				memcpy(circleBuf->buffer + writeIndex, data, circleBuf->dataLen - writeIndex);
				memcpy(circleBuf->buffer, data + circleBuf->dataLen - writeIndex, writeIndex + length - circleBuf->dataLen);
				circleBuf->writeIndex = writeIndex + length - circleBuf->dataLen;
				ret = length;
			}else{													//---r--l---------------------w-
				//printf("full, w+l > r\n");
				ret = 0;
			}
		}else{														//-----r-----------------w----l--
			memcpy(circleBuf->buffer + writeIndex, data, length);
			circleBuf->writeIndex += length;
			ret = length;
		}
	}else if (writeIndex < readIndex){								
		if (writeIndex + length >= readIndex){						//------------w--r--l-----------搴斿綋涓㈠け锛屾姤閿欙紝杩斿洖0
			//printf("full, w+l > r\n");
			ret = 0;
		}else{														//------------w---l---r----------姝ｅ父锛岃繑鍥瀕
			memcpy(circleBuf->buffer + writeIndex, data, length);
			circleBuf->writeIndex += length;
			ret = length;
		}
	}
	pthread_mutex_unlock(&circleBuf->mutex);
	
	return ret;
}
int zfifo_check(char *handle)
{
	int len;
	zfifo* p = (zfifo*)handle;
	int write = p->writeIndex;
	int read = p->readIndex;
	if(write >= read)len = write - read;
	else{
		len = p->dataLen - (read - write);
	}
	return len;
}
int zfifo_clear(char* handle)
{
	zfifo* circleBuf = (zfifo*)handle;
	pthread_mutex_lock(&circleBuf->mutex);
	circleBuf->readIndex = 0;
	circleBuf->writeIndex = 0;
	circleBuf->allowWrite = TRUE;
	pthread_mutex_unlock(&circleBuf->mutex);
	return 0;
}
int zfifo_clearA(char* handle)
{
	zfifo* circleBuf = (zfifo*)handle;
	pthread_mutex_lock(&circleBuf->mutex);
	//read from writeIndex to readIndex while the data == 0x11111 then stop 
	int writepos = circleBuf->writeIndex;
	int readpos = circleBuf->readIndex;
	int all = circleBuf->dataLen;
	int i;
	char header[2];
	if(writepos >= readpos){
		for(i=writepos;i>(readpos+1);i--){
			header[0] = circleBuf->buffer[i-1];
			header[1] = circleBuf->buffer[i];
			if (header[0] == 0xFF && header[1]  == 0xF1){
				circleBuf->readIndex = i-1;
				break;
			}			
		}
	}else{
		for(i=writepos;i>1;i--){
			header[0] = circleBuf->buffer[i-1];
			header[1] = circleBuf->buffer[i];
			if (header[0] == 0xFF && header[1]  == 0xF1){
				circleBuf->readIndex = i-1;
				break;
			}
		}
		for(i=all;i>readpos+1;i--){
			header[0] = circleBuf->buffer[i-1];
			header[1] = circleBuf->buffer[i];
			if (header[0] == 0xFF && header[1]  == 0xF1){
				circleBuf->readIndex = i-1;
				break;
			}			
		}
	}
	circleBuf->allowWrite = TRUE;
	printf("clear = %02x %02x\n",circleBuf->buffer[circleBuf->readIndex],circleBuf->buffer[circleBuf->readIndex+1]);
	pthread_mutex_unlock(&circleBuf->mutex);
	return 0;
}

/*
int main(int argc, char* argv[])
{
	char* buff;
	char data[20];
	zfifo_init(&buff, 100000);
	zfifo_write(buff, "131213232424",10);
	zfifo_read(buff,data,9);
	data[19] = '\0';
	printf("%s",data);
	zfifo_destroy(buff);
}
int main(int argc, char* argv[])
{
	char* buff;
	char data[20]="awefnweeeaf";
	char *ptr;int len;int type;unsigned long long pts;

	ringfifo_init(&buff, 100000);

	ringfifo_write(buff, data,10, 1, 111111);
	ringfifo_write(buff, data,9, 2, 111112);
	

	ringfifo_read(buff,&ptr, &len, &type, &pts);
	printf("%s len=%d type=%d pts=%ld\n",ptr,len,type,pts);
	ringfifo_read(buff,&ptr, &len, &type, &pts);
	printf("%s len=%d type=%d pts=%ld\n",ptr,len,type,pts);
	ringfifo_destroy(buff);
}*/


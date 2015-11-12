#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "lib_public.h"
#include "lib_net.h"
#include <sys/time.h>
#define PORT 9999
#define IPADDR "127.0.0.1"
int packet(char *sendbuf, int cmd, int len, char *data);
int main(int argc, char *argv[])
{
    /*  套接字描述符*/
    int sockfd, numbytes;
	char buf[1024];
    struct sockaddr_in their_addr;
	int i, ret = 0, slen = 0;
	int cnt = 0;
	for(;cnt < 500; ++cnt)
	{
		if ((sockfd = socket(AF_INET,SOCK_STREAM, 0)) == -1) 
		{
			perror("socket");
			exit(1);
		}

		their_addr.sin_family = AF_INET;
		/*  网络字节顺序，短整型*/
		their_addr.sin_port = htons(PORT);
		their_addr.sin_addr.s_addr = inet_addr(IPADDR);
		/*  将结构剩下的部分清零*/
		bzero(&(their_addr.sin_zero),8);
		printf("ready to connect\n");
		if(connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1)
		{
			perror("connect");
			exit(1);
		}
		char *senddata = "hahanihao";

		// slen = strlen(senddata);
		// ret = packet(buf, 0x0001, slen, senddata);
		ret = lib_tcp_send(sockfd, senddata, strlen(senddata));
		if(ret > 0)
		{
			printf("send ok:%d\n", cnt);
		}
		else
		{
			printf("send to server error\n");
		}
		usleep(100);
	}
    getchar();
    return 0;
}

int packet(char *sendbuf, int cmd, int len, char *data)
{
	static unsigned short frame;
	unsigned short crc = 0;
	sendbuf[0] = 0xff;
	sendbuf[1] = 0xff;  //start addr
	memcpy(sendbuf+2, "000000", 6); //source addr
	memcpy(sendbuf+8, "000000", 6); //destination addr
	sendbuf[14] = cmd /256;
	sendbuf[15] = cmd % 256;  //cmd
	if(++frame > 65535) frame = 0;
	sendbuf[16] = frame /256;
	sendbuf[17] = frame % 256; //frame
	sendbuf[18] = len /256;
	sendbuf[19] = len % 256;  //data len
	memcpy(sendbuf+20, data, len);

	crc = lib_crc_check(sendbuf+2, len+18);
	sendbuf[20 + len] = crc /256;
	sendbuf[20 + len + 1] = crc % 256;
	
	return len + 22;
}

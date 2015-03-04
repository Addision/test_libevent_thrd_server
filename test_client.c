#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define PORT 9999
#define IPADDR "127.0.0.1"
int main(int argc, char *argv[])
{
    /*  套接字描述符*/
    int sockfd, numbytes;
	char buf[1024];
    struct sockaddr_in their_addr;  

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
    if(connect(sockfd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("connect");
        exit(1);
    }
	int ret = send(sockfd, "123456", 6,0);
	if(ret > 0)
	{
		printf("send ok\n");
	}
	ret = recv(sockfd, buf, 1024,0);
	if(ret > 0)
	{
		printf("recv data %s\n", buf);
	}
    getchar();
    return 0;
}

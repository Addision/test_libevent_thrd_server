#include "lib_net.h"
#include "lib_thread.h"
#include "lib_public.h"
#include<event.h>  
#include<event2/util.h>

#define BACKLOG 10
#define MAX_EVENTS 500

char ip[24];
short port;

struct st_listenserv
{
	int sockfd;
	struct event *ev_listen;
	struct event_base *base;
};

struct st_connserv
{
	int clifd;
	struct event_base *base;
	struct event *ev_read;
	struct event *ev_write;
	char buf[1024];
};

void initsocket(struct st_listenserv *listenserv);
void accept_cb(int fd, short events, void *arg);
void read_cb(int fd, short events, void *arg);
void send_cb(int fd, short events, void *arg);
void start_thrd(int fd);
void thrd_readwrite(int sockfd);
void release_read(struct st_connserv *connserv);
void release_write(struct st_connserv *connserv);

int main(int argc, char *argv[])
{
	
	if(argc < 1)
	{
		perror("input server  port");
		return -1;
	}
	memcpy(ip, argv[1], 24);
	port = atoi(argv[2]);
	struct st_listenserv listenserv;
	initsocket(&listenserv);
	listenserv.base = event_base_new();
	listenserv.ev_listen = event_new(listenserv.base, listenserv.sockfd,EV_READ | EV_PERSIST,accept_cb,NULL);
	event_add(listenserv.ev_listen, NULL);
	event_base_dispatch(listenserv.base);
	
}

void initsocket(struct st_listenserv *listenserv)
{
	listenserv->sockfd = lib_tcpsrv_init(ip,port);
	if(listenserv->sockfd < 0)
	{
		perror("server create socket error");
		return;
	}
	if(lib_set_nonblock(listenserv->sockfd) < 0)
	{
		perror("set nonblock error");
		return;
	}
	printf("init socket ok\n");
}

void accept_cb(int fd, short events, void *arg)
{
	struct sockaddr_in cin;
	socklen_t socklen = sizeof(cin);
	int clifd = lib_tcpsrv_accept(fd, &cin);
	if(clifd <0)
	{
		perror("accept error");
	}
    printf("new conneciotn [%s:%d]\n", inet_ntoa(cin.sin_addr), ntohs(cin.sin_port));
	start_thrd(clifd);
}

void start_thrd(int fd)
{
	pthread_t pid;
	lib_thread_create(&pid, thrd_readwrite, (void*)fd, 1);	
}

void thrd_readwrite(int sockfd)
{

	struct st_connserv connserv;
	connserv.clifd = sockfd;
	connserv.base = event_base_new();
	connserv.ev_read = event_new(connserv.base, connserv.clifd, EV_READ|EV_PERSIST, read_cb, &connserv);
	event_add(connserv.ev_read,NULL);
	event_base_dispatch(connserv.base);
}

void read_cb(int fd, short events, void *arg)
{

	struct st_connserv *conn = (struct st_connserv*)arg;
	int recvlen = lib_tcp_recv(conn->clifd, conn->buf,1024, -1);
	if(recvlen < 0)
	{
		perror("recv error");
		close(conn->clifd);
		release_read(conn);
	}
	printf("recv data:%s\n", conn->buf);
	
	conn->ev_write = event_new(conn->base, conn->clifd, EV_WRITE|EV_PERSIST, send_cb, conn);
	event_add(conn->ev_write, NULL);
}

void send_cb(int fd, short events, void *arg)
{
	struct st_connserv *conn = (struct st_connserv*)arg;
	int sendlen = lib_tcp_send(conn->clifd, conn->buf, 1024);
	if(sendlen < 0)
	{
		perror("send error");
		close(conn->clifd);
		release_write(conn);
	}
}

void release_read(struct st_connserv *connserv)
{
	if(connserv == NULL)
	{
		return;
	}
	event_del(connserv->ev_read);
	event_base_loopexit(connserv->base, NULL);
	if(NULL != connserv->ev_read){
		free(connserv->ev_read);
	}
	event_base_free(connserv->base);
	// destroy_sock_ev_write(sock_ev_struct->sock_ev_write_struct);
	free(connserv);
	
}

void release_write(struct st_connserv *connserv)
{
	if(connserv != NULL)
	{
		free(connserv->ev_write);
		free(connserv);
	}
}

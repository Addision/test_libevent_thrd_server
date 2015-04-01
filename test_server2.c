#include<event.h>  
#include<event2/util.h>
#include<signal.h>
#include <string.h>
#include <stdlib.h>
#include "lib_net.h"
#include "lib_thread.h"
#include "lib_public.h"
#include "lib_file.h"

struct st_listenserv
{
	int sockfd;
	char ip[24];
	short port;
	int backlog;
	int maxevents;
	int thrdnum;
	struct event *ev_listen;
	struct event_base *base;
};

struct st_thrd_work
{
	int clifd;
	pthread_t pid;
	int status;
	struct event_base *base;
	struct event *ev_read;
	struct event *ev_write;
	char buf[1024];
};

int last_active = 0;
struct st_thrd_work *st_thrd;
int cnt = 0;
void initsocket(struct st_listenserv *listenserv);
void accept_cb(int fd, short events, void *arg);
void send_cb(int fd, short events, void *arg);
void release_read(struct st_thrd_work *thrd_work);
void release_write(struct st_thrd_work *thrd_work);
void thrd_work_cb(int fd, short events, void *arg);
void thrd_work(struct st_thrd_work *thrd_work);
void thrd_work_process(void *arg);
void initst_listenserv(struct st_listenserv *listenserv);

int main(int argc, char *argv[])
{
	int i=0;
	char tmpbuf[30];
	sigset(SIGPIPE, SIG_IGN);

	struct st_listenserv listenserv;
	memset(&listenserv, 0 ,sizeof(listenserv));
	initst_listenserv(&listenserv);
	initsocket(&listenserv);
    //创建线程池
	st_thrd = calloc(listenserv.thrdnum, sizeof(struct st_thrd_work));
	for(i=0; i<listenserv.thrdnum; ++i)
	{
		st_thrd[i].base = event_base_new();
	}
	for(i=0; i<listenserv.thrdnum; ++i)
	{
		thrd_work(&st_thrd[i]);
	}
	
	listenserv.base = event_base_new();
	listenserv.ev_listen = event_new(listenserv.base, listenserv.sockfd,EV_READ | EV_PERSIST,accept_cb,NULL);
	event_add(listenserv.ev_listen, NULL);
	event_base_dispatch(listenserv.base);
	
}

void initsocket(struct st_listenserv *listenserv)
{
	listenserv->sockfd = lib_tcpsrv_init(listenserv->ip,listenserv->port);
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
	printf("server accept\n");
	struct st_listenserv *listenserv = arg;
	struct sockaddr_in cin;
	socklen_t socklen = sizeof(cin);
	int clifd = lib_tcpsrv_accept(fd, &cin);
	if(clifd <0)
	{
		perror("accept error");
	}
	if(lib_set_nonblock(clifd) < 0)
	{
		perror("set nonblock error");
		return;
	}
	cnt++;
    printf("new conneciotn [%s:%d] %d\n", inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), cnt);
 
	int tid = (++last_active) % listenserv->thrdnum;
	struct st_thrd_work *thrd = st_thrd + tid;
	last_active = tid;
	thrd->clifd = clifd;
	printf("{%lu : %d}\n", thrd->pid, thrd->clifd);
    
	thrd->ev_read = event_new(thrd->base, thrd->clifd, EV_READ|EV_PERSIST|EV_TIMEOUT, thrd_work_cb, thrd);

	event_add(thrd->ev_read, NULL);
	
    if(last_active > 1000)
		last_active = 0;
}
 
void thrd_work_cb(int fd, short events, void *arg)
{
	struct st_thrd_work *thrd_work = (struct st_thrd_work *)arg;
	//有事件时进行处理
	int recvlen = 0;
	if( thrd_work != NULL)
	{
   		// recvlen = lib_tcp_recv(thrd_work->clifd, thrd_work->buf,1024, -1);
		// if(recvlen < 0)
		// {
		// 	perror("recv error");	
		// 	close(thrd_work->clifd);
		// 	release_read(thrd_work);
		// 	return;
		// }
		// printf("recv data:%s\n", thrd_work->buf);
		// memset(thrd_work->buf, 0, sizeof(thrd_work->buf));
		
		memset(thrd_work->buf, 0, sizeof(thrd_work->buf));
		while(1)
		{
			recvlen = lib_tcp_recv(thrd_work->clifd, thrd_work->buf, 2, -1);
			if((recvlen !=2) || ((unsigned char)thrd_work->buf[0] != 0xFF) || ((unsigned char)thrd_work->buf[1] != 0xFF))
			{
				perror("server recv error");
				close(thrd_work->clifd);
				release_read(thrd_work);
				return;
			}
			else
				break;
		}
		recvlen = lib_tcp_recv(thrd_work->clifd, thrd_work->buf+2, 18, -1);
		char addr[6];
		memcpy(addr, thrd_work->buf+2, 6);
		printf("source addr %s\n", addr);
		memset(addr, 0 , sizeof(addr));
		memcpy(addr, thrd_work->buf+8, 6);
		printf("destination addr %s\n", addr);
		int cmd = (unsigned char)thrd_work->buf[14]*256 + (unsigned char)thrd_work->buf[15];
		switch(cmd)
		{
		case 0x0001:
			printf("success recv cmd\n");
			
			break;
		default:
			break;
		}
		int datalen = (unsigned char)thrd_work->buf[18]*256 + (unsigned char)thrd_work->buf[19];
		if(recvlen < 0)
		{
			perror("server recv error");
			close(thrd_work->clifd);
			release_read(thrd_work);
			return;
		}
		recvlen = lib_tcp_recv(thrd_work->clifd, thrd_work->buf+20, datalen+2, -1);
		unsigned short crc = 0;
		crc = lib_crc_check(thrd_work->buf+2, datalen+18);
		if(crc != (unsigned char)thrd_work->buf[20 + datalen]*256 + (unsigned char)thrd_work->buf[21 + datalen])
		{
			perror("crc error");
			return ;
		}
		char datarecv[256];
		memcpy(datarecv, thrd_work->buf+20,datalen);
      	printf("recv data %s\n", datarecv);
		// thrd_work->ev_write = event_new(thrd_work->base, thrd_work->clifd, EV_WRITE, send_cb, thrd_work);
		// if(thrd_work->ev_write != NULL)
		//     event_add(thrd_work->ev_write, NULL);
	}

}

void thrd_work_process(void *arg)
{
	struct st_thrd_work *st_work = (struct st_thrd_work *)arg;
	printf("====%lu====\n", st_work->pid);
	do
	{
		if(st_work->ev_read != NULL)
		{
			event_base_dispatch(st_work->base);
		}
		else
			sleep(1);
		
   } while (1);
    
    event_base_free(st_work->base);
}

void thrd_work(struct st_thrd_work *st_work)
{
	
	if(st_work != NULL)
	{
		lib_thread_create(&(st_work->pid), thrd_work_process, st_work, 1);
	}
}

void send_cb(int fd, short events, void *arg)
{
	struct st_thrd_work *thrd_work = (struct st_thrd_work*)arg;
	if(thrd_work->clifd <= 0)
		return;
	int sendlen = lib_tcp_send(thrd_work->clifd, thrd_work->buf, 1024);
	if(sendlen < 0)
	{
		perror("send error");		
	    close(thrd_work->clifd);
		release_write(thrd_work);
	}
	memset(thrd_work->buf, 0, sizeof(thrd_work->buf));
}

void release_read(struct st_thrd_work *thrd_work)
{
	if(thrd_work == NULL)
	{
		return;
	}
	if(NULL != thrd_work->ev_read)
	{
		event_del(thrd_work->ev_read);
		event_free(thrd_work->ev_read);

	}
}

void release_write(struct st_thrd_work *thrd_work)
{
	if(thrd_work == NULL)
		return;
	if(thrd_work->ev_write != NULL)
	{
		 event_free(thrd_work->ev_write);
	}
}

void initst_listenserv(struct st_listenserv *listenserv)
{
	char tmpbuf[30];
	memset(tmpbuf, 0, sizeof(tmpbuf));
	lib_file_readcfg("server.ini","[net]", "ip", listenserv->ip);
	lib_file_readcfg("server.ini", "[net]", "ip", tmpbuf);
	listenserv->port = atoi(tmpbuf);
	memset(tmpbuf, 0, sizeof(tmpbuf));
	lib_file_readcfg("server.ini","[thread]","num", tmpbuf);
	listenserv->thrdnum = atoi(tmpbuf);
}

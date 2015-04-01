#include "lib_net.h"
#include "lib_thread.h"
#include "lib_public.h"
#include<event.h>  
#include<event2/util.h>
#include<signal.h>

#define BACKLOG 10
#define MAX_EVENTS 500
#define THRD_NUM 5

char ip[24];
short port;

struct st_listenserv
{
	int sockfd;
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

int main(int argc, char *argv[])
{
	int i=0;
	
	sigset(SIGPIPE, SIG_IGN);
	if(argc < 3)
	{
		perror("input server  port");
		return -1;
	}
	memcpy(ip, argv[1], 24);
	port = atoi(argv[2]);
	struct st_listenserv listenserv;
	initsocket(&listenserv);
    //创建线程池
	st_thrd = calloc(THRD_NUM, sizeof(struct st_thrd_work));
	for(i=0; i<THRD_NUM; ++i)
	{
		st_thrd[i].base = event_base_new();
	}
	for(i=0; i<THRD_NUM; ++i)
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
	char ti[30];
	struct sockaddr_in cin;
	socklen_t socklen = sizeof(cin);
	int clifd = lib_tcpsrv_accept(fd, &cin);
	if(clifd <0)
	{
		perror("accept error");
		exit(-1);
	}
	if(lib_set_nonblock(clifd) < 0)
	{
		perror("set nonblock error");
		return;
	}
	cnt++;
    printf("[%s]:new conneciotn [%s:%d] %d\n", lib_time_now(ti, 0),inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), cnt);

 
	int tid = (++last_active) % THRD_NUM;
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
   		recvlen = lib_tcp_recv(thrd_work->clifd, thrd_work->buf,1024, -1);
		if(recvlen < 0)
		{
			perror("recv error");	
			close(thrd_work->clifd);
			release_read(thrd_work);
			return;
		}
		printf("recv data:%s\n", thrd_work->buf);
		memset(thrd_work->buf, 0, sizeof(thrd_work->buf));
		
		thrd_work->ev_write = event_new(thrd_work->base, thrd_work->clifd, EV_WRITE, send_cb, thrd_work);
		if(thrd_work->ev_write != NULL)
		    event_add(thrd_work->ev_write, NULL);
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
			usleep(100);
		
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


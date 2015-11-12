#include <event.h>  
#include <event2/util.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
#include "lib_net.h"
#include "lib_thread.h"
#include "lib_public.h"
#include "lib_file.h"

struct st_listenserv
{
	int sockfd;
	char ip[24];
	short port;
	int thrdnum;
	struct event *ev_listen;
	struct event_base *base;
	pthread_t pid;
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

static pthread_mutex_t lock;
void initsocket(struct st_listenserv *listenserv);
void accept_cb(int fd, short events, void *arg);
void send_cb(int fd, short events, void *arg);
void release_read(struct st_thrd_work *thrd_work);
void release_write(struct st_thrd_work *thrd_work);
void thrd_work_cb(int fd, short events, void *arg);
void thrd_work(struct st_thrd_work *thrd_work);
void thrd_work_process(void *arg);
void initst_listenserv(struct st_listenserv *listenserv);
void ReleaseEvent(struct event* ev);

int main(int argc, char *argv[])
{
	int i=0;
	
	sigset(SIGPIPE, SIG_IGN);

	const char* version = event_get_version();
	printf("the libevent verison is %s\n", version);

	//change rlimit
	struct rlimit rlim;
	struct rlimit rlim_new;
	if(getrlimit(RLIMIT_CORE, &rlim) == 0)
	{
		rlim_new.rlim_cur = rlim_new.rlim_max = RLIM_INFINITY;
		if(setrlimit(RLIMIT_CORE, &rlim_new) !=0)
		{
			rlim_new.rlim_cur = rlim_new.rlim_max = rlim.rlim_max;
			setrlimit(RLIMIT_CORE, &rlim_new);
		}
	}

	if((getrlimit(RLIMIT_CORE, &rlim) != 0) || rlim.rlim_max == 0)
	{
		perror( "Set Resource limit failed\n");
		exit(-1);
	}
	
	struct st_listenserv listenserv;
	memset(&listenserv, 0 ,sizeof(listenserv));
	initst_listenserv(&listenserv);
	initsocket(&listenserv);
	pthread_mutex_init(&lock, NULL);
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
	listenserv.ev_listen = event_new(listenserv.base, listenserv.sockfd, EV_READ|EV_PERSIST, (void*)accept_cb, &listenserv);
	event_add(listenserv.ev_listen, 0);
	event_base_dispatch(listenserv.base);
	event_base_free(listenserv.base);
	return 0;
}

void initsocket(struct st_listenserv *listenserv)
{
	listenserv->sockfd = lib_tcpsrv_init(listenserv->ip,listenserv->port);
	if(listenserv->sockfd < 0)
	{
		perror("server create socket error");
		exit(-1);
	}
	if(lib_set_nonblock(listenserv->sockfd) < 0)
	{
		perror("set nonblock error");
		exit(-1);
	}
	printf("init socket ok\n");
}

void accept_cb(int fd, short events, void *arg)
{
	char ti[30];
	struct st_listenserv *listenserv = (struct st_listenserv*)arg;
	struct sockaddr_in cin;
	//socklen_t socklen = sizeof(cin);
	int clifd = lib_tcpsrv_accept(fd, &cin);
	if(clifd <0)
	{
		perror("accept error");
		exit(-1);
	}
	if(lib_set_nonblock(clifd) < 0)
	{
		perror("set nonblock error");
		exit(-1);
	}
	cnt++;
    printf("[%s] new connection [%s:%d] %d\n", lib_time_now(ti, 0),inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), cnt);

	int tid = (++last_active) % listenserv->thrdnum;
	struct st_thrd_work *thrd = st_thrd + tid;
	last_active = tid;
	thrd->clifd = clifd;
	printf("{%lu : %d}\n", thrd->pid, thrd->clifd);
	thrd->ev_read = event_new(thrd->base, thrd->clifd, EV_READ|EV_PERSIST|EV_TIMEOUT, thrd_work_cb, thrd);
	event_add(thrd->ev_read, 0);
	
    if(last_active > 1000)
		last_active = 0;
}
 
void thrd_work_cb(int fd, short events, void *arg)
{
	struct st_thrd_work *thrd_work = (struct st_thrd_work *)arg;
	//有事件时进行处理
	int recvlen = 0;
	char datarecv[256];
	int sockfd = fd;

	if( thrd_work != NULL && sockfd > 0)
	{
 		memset(datarecv, 0, sizeof(datarecv));
		do
		{
			recvlen = lib_tcp_recv(sockfd, datarecv, 9, 0);
			if(recvlen <= 0)
			{
				if(errno == EAGAIN)
				{
				    continue;
				}
				perror("recv the data failed\n");			
				close(sockfd);
				sockfd = -1;
				ReleaseEvent(thrd_work->ev_read);
				return;
			}
			else if(recvlen > 0)
			{
				printf("recv data %s\n", datarecv);
				break;
			}				

		} while (1);

		// thrd_work->ev_write = event_new(thrd_work->base, thrd_work->clifd, EV_WRITE, send_cb, thrd_work);
		// if(thrd_work->ev_write != NULL)
		//     event_add(thrd_work->ev_write, 0);
	}

}

void thrd_work_process(void *arg)
{
	struct st_thrd_work *st_work = (struct st_thrd_work *)arg;
	printf("the work thread id: %lu\n", st_work->pid);

	do
	{
		if(st_work->ev_read != NULL || st_work->ev_write != NULL)
		{
			if(event_base_dispatch(st_work->base) == -1)
			{
				perror("the work thread error");
				break;
			}
		}
	} while (1);

    event_base_free(st_work->base);
	return;
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
	int sockfd = thrd_work->clifd;
	if(sockfd <= 0)
		return;
	int sendlen = lib_tcp_send(sockfd, thrd_work->buf, 1024);
	if(sendlen < 0)
	{
		release_write(thrd_work);
		close(sockfd);
		thrd_work->clifd = -1;
	}
	memset(thrd_work->buf, 0, sizeof(thrd_work->buf));
}

void release_read(struct st_thrd_work *thrd_work)
{
	struct event * ev = thrd_work->ev_read;

	if(thrd_work == NULL)
	{
		return;
	}
	if(NULL != ev)
	{
		event_del(ev);
	}
}

void release_write(struct st_thrd_work *thrd_work)
{
	if(thrd_work == NULL)
		return;
	if(thrd_work->ev_write != NULL)
	{
		event_del(thrd_work->ev_write);
	}
}

void initst_listenserv(struct st_listenserv *listenserv)
{
	char tmpbuf[50];
	memset(tmpbuf, 0, sizeof(tmpbuf));
	lib_file_readcfg("server.ini","[net]", "ip", listenserv->ip);
	lib_file_readcfg("server.ini","[net]", "port", tmpbuf);
	listenserv->port = atoi(tmpbuf);
	memset(tmpbuf, 0, sizeof(tmpbuf));
	lib_file_readcfg("server.ini","[thread]","num", tmpbuf);
	listenserv->thrdnum = atoi(tmpbuf);
	printf("init listenserv ok\n");
	listenserv->pid = pthread_self();
}

void ReleaseEvent(struct event* ev)
{
	if(ev != NULL)
	{
		if(event_del(ev) < 0)
		{
			perror("event del err\n");
			return;
		}
	}
}

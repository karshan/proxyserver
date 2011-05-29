#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/types.h>
#include <regex.h>

#include "http.h"

int max ( int a, int b, int c) {
	int x = (a > b)?a:b;
	return (c > x)?c:x;
}

typedef enum {
	BadRequest = 0,
	NotFound
} HTTPResponse_enum;

#define MAXURLSIZE 100

HTTPBuffer getHTTPResponse(HTTPResponse_enum r) {
	/* TODO: Maybe add a header, Connection: close in the response ?? */
	HTTPBuffer response;
	const char *content;
	char *cptr = response.getbuf();

	if(r == BadRequest) {
		content = "<html><head><title>HTTP/400 Bad Request</title></head><body><h1>Bad Request</h1><div>Karshan's proxy server</div></body></html>";
		snprintf(cptr, BUFSIZE, "HTTP/1.1 400 Bad Request\r\nContent-type: text/html\r\nContent-Length: %d\r\n\r\n%s", (int)strlen(content), content);
		response.setlen(strlen(cptr));
	}
	else /*if(r == Notfound) */ {
		content = "<html><head><title>HTTP/404 Not Found</title></head><body><h1>Not Found</h1><div>Karshan's proxy server</div></body></html>";
		snprintf(cptr, BUFSIZE, "HTTP/1.1 404 Bad Request\r\nContent-type: text/html\r\nContent-Length: %d\r\n\r\n%s", (int)strlen(content), content);
		response.setlen(strlen(cptr));
	}	
	return response;
}

char *urlfromrequest(char* url, const HTTPBuffer req) {
	int i;
	char *urlp = strstr(req.getbuf(), "Host:");
	if(urlp == NULL) 
		return NULL;
	urlp += 5; /* strlen("Host:"); */
	while((*urlp == ' ') || (*urlp == '\t')) urlp++;

	for(i = 0;(i < MAXURLSIZE) && (*urlp != '\r') && (*urlp != '\n') && (*urlp != '\0') && (*urlp != ' ');i++, urlp++)
		url[i] = *urlp;
	url[i] = '\0';
	return url;
}	

int connectto(char *host, const char *port = "80") {
	int gai_errno, connectfd = -1;
	struct addrinfo hints, *saddrinfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	if((gai_errno = getaddrinfo(host, port, &hints, &saddrinfo)) != 0) {
		printf("getaddrinfo: %s\n", gai_strerror(gai_errno));
		return -1;
	}

	struct addrinfo *p = saddrinfo;
	do {
		if(p == NULL) {
			freeaddrinfo(saddrinfo);
			return -1;
		}		
		connectfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(connectfd != -1) {
			if(connect(connectfd, p->ai_addr, p->ai_addrlen) == -1) {
				printf("connect: %s\n", strerror(errno));
				close(connectfd);
				connectfd = -1;
			}
		}
		p = p->ai_next;
	}while(connectfd == -1);
	freeaddrinfo(saddrinfo); /*We're now connected to the server*/
	return connectfd;
}

int sockfd = -1;
int logfd = -1;
int kpipe[2] = {-1, -1};

volatile int child_count = 0;
int ctid = 0;
pthread_mutex_t child_count_mutex = PTHREAD_MUTEX_INITIALIZER;

void sigint_handler(int signum) {
	printf("\b\bCaught signal %d\n", signum);
	if(kpipe[1] != -1)
		if(write(kpipe[1], "d", 1) == -1)
			perror("write");
}

typedef struct conn_handler_args {
	int tid, fd;
	pthread_t *t_ptr;
	struct sockaddr_storage client_addr;
}conn_handler_args_t;

void *conn_handler(void *);

int usage(char * progname) {
	printf("usage: %s <port>\n", progname);
	return 1;
}

int main(int argc, char **argv) {
	if(argc < 2)
		return usage(argv[0]);

	signal(SIGINT, sigint_handler);
	
	conn_handler_args_t *conn_handler_arg = NULL;
	struct addrinfo hints, *res = NULL;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	int acceptfd;
	struct sockaddr_storage client_addr;
	socklen_t addr_size = sizeof(struct sockaddr_storage);
	int reuse_addr = 1;
	int gai_errno;
	
	if((logfd = open("log", O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR)) == -1) {
		printf("open: %s\n", strerror(errno));
		goto cleanup;
	}

	if(pipe(kpipe) == -1) {
		perror("pipe");
		goto cleanup;
	}

	pthread_attr_t attr;
	if(pthread_attr_init(&attr)) {
		printf("pthread_attr_init failed!\n");
		goto cleanup;
	}
	if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)) {
		printf("pthread_attr_setdetachstate failed!\n");
		goto cleanup;
	}

	if((gai_errno = getaddrinfo(NULL, argv[1], &hints, &res)) != 0) {
		printf("getaddrinfo: %s\n", gai_strerror(gai_errno));
		goto cleanup;
	}
	
	if((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		printf("socket: %s\n", strerror(errno));
		goto cleanup;
	}
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
	if(bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		printf("bind: %s\n", strerror(errno));
		goto cleanup;
	}
	if(listen(sockfd, 32) == -1) { /*32 is the queue length*/
		printf("listen: %s\n", strerror(errno));
		goto cleanup;
	}
	if(fcntl(sockfd, F_SETFL, O_NONBLOCK) == -1) {
		perror("fcntl");
		goto cleanup;
	}
	
	while(1) {
		acceptfd = -1;
		conn_handler_arg = NULL;
		
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(sockfd, &rfds);
		FD_SET(kpipe[0], &rfds);
		
		if(select((sockfd > kpipe[0]) ? (sockfd + 1):(kpipe[0] + 1), &rfds, NULL, NULL, NULL) == -1) {
			if(errno == EINTR)
				continue;
			perror("select");
			goto cleanup;
		}
		if(FD_ISSET(kpipe[0], &rfds))
			goto cleanup;
		if((acceptfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_size)) == -1) {
			printf("accept: %s\n", strerror(errno));
			goto cleanup;
		}
		if(fcntl(acceptfd, F_SETFL, O_NONBLOCK) == -1) {
			perror("fcntl");
			goto cleanup;
		}

		conn_handler_arg = (conn_handler_args_t*)malloc(sizeof(conn_handler_args_t));
		if(conn_handler_arg == NULL) {
			printf("malloc failed!\n");
			goto cleanup;
		}
		conn_handler_arg->t_ptr = (pthread_t*)malloc(sizeof(pthread_t*));
		if(conn_handler_arg->t_ptr == NULL) {
			printf("malloc failed!\n");
			goto cleanup;
		}

		conn_handler_arg->tid = ctid++;
		conn_handler_arg->fd = acceptfd;
		conn_handler_arg->client_addr = client_addr;
		if(pthread_create(conn_handler_arg->t_ptr, &attr, conn_handler, conn_handler_arg)) {
			printf("pthread_create failed!\n");
			goto cleanup;
		}
		child_count++;
	}

cleanup:
	while(child_count); /* wait for all threads to finish there work */
	if(res != NULL)
		freeaddrinfo(res);
	if(pthread_attr_destroy(&attr))
		printf("pthread_attr_destroy failed, (maybe irrelevant:) %s\n", strerror(errno));
	if(acceptfd != -1)
		close(acceptfd);
	if(conn_handler_arg != NULL) {
		free(conn_handler_arg->t_ptr); /* freeing NULL isn't bad */
		free(conn_handler_arg);
	}
	if(sockfd != -1)
		if(close(sockfd) == -1)
			perror("close sockfd");
	if(logfd != -1)
		close(logfd);
	if(kpipe[0] != -1)
		close(kpipe[0]);
	if(kpipe[1] != -1)
		close(kpipe[1]);
	return 0;
}

void* conn_handler(void *arg) {
	conn_handler_args_t *conn_handler_arg = (conn_handler_args_t*)arg;
	int tid = conn_handler_arg->tid; 	
	printf("conn_handler() %d begin\n", tid);	
	printf("thread count: %d\n", child_count);

	char url[MAXURLSIZE];
	int connectfd = -1;
	int n;
	HTTPBuffer request, resp;
	int fd = conn_handler_arg->fd;
	fd_set rfds;
	while(1) {
		FD_ZERO(&rfds);
		FD_SET(kpipe[0], &rfds);
		FD_SET(fd, &rfds);
		if(connectfd != -1)
			FD_SET(connectfd, &rfds);
		
		if(select(max(fd, kpipe[0], connectfd) + 1, &rfds, NULL, NULL, NULL) == -1) {
			if(errno == EINTR) 
				continue;
			perror("select");
			goto cleanup;
		}

		if(FD_ISSET(kpipe[0], &rfds))
			goto cleanup;

		if(FD_ISSET(fd, &rfds)) {
			// recv() request from client
			if((n = request.recv(fd)) == -1) {
				perror("recv");
				goto cleanup;
			}
			if(n == 0) //client closed connection
				goto cleanup;

			if(request.iscompleterequest()) {
				request.log(logfd);

				/* we have a complete request */
				if(urlfromrequest(url, request) == NULL) {
					resp = getHTTPResponse(BadRequest);
					resp.send(fd);
					goto cleanup;
				}
				if(connectfd != -1)
					close(connectfd);
				connectfd = connectto(url);
				
				if(connectfd == -1) {
					printf("connect fd failed!\n");
					resp = getHTTPResponse(NotFound);
					resp.send(fd);
					goto cleanup;
				}
				if(request.send(connectfd) == -1) {
					resp = getHTTPResponse(NotFound);
					resp.send(fd);
					goto cleanup;
				}
				request.reset();
			}
			/* TODO:else{ send HTTP\1.1 continue .. or whatever }*/
		}
		else if(FD_ISSET(connectfd, &rfds)) {
			if((n = resp.recv(connectfd)) == -1) {
				perror("recv");
				goto cleanup;
			}
			if(n == 0) {
				/* server closed connection */
				close(connectfd);
				connectfd = -1;	
			}
			else {
				/* TODO: only send response once its fully recieved! */
				resp.send(fd);
				resp.reset();	
			}
		}
	}
cleanup:
	printf("conn_handler() %d end\n", conn_handler_arg->tid);
	if(close(fd) == -1)
		perror("close fd");
	if(connectfd != -1)
		close(connectfd);
	free(conn_handler_arg->t_ptr);
	free(conn_handler_arg);	
	printf("thread count: %d\n", child_count);
	pthread_mutex_lock(&child_count_mutex);
		child_count--;
	pthread_mutex_unlock(&child_count_mutex);
	return NULL;
}


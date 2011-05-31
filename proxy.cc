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

#include <iostream>
using namespace std; //todo get rid of printfs

int max ( int a, int b, int c) {
	int x = (a > b)?a:b;
	return (c > x)?c:x;
}

#define MAXURLSIZE 100
/*
HTTPBuffer getHTTPResponse(HTTPResponse_enum r) {
	 TODO: Maybe add a header, Connection: close in the response ?? */
	/*HTTPBuffer response;
	const char *content;
	char *cptr = response.getbuf();
	//TODO optimize, use snprintf's return val instead of strlen
	if(r == BadRequest) {
		content = "<html><head><title>HTTP/400 Bad Request</title></head><body><h1>Bad Request</h1><div>Karshan's proxy server</div></body></html>";
		snprintf(cptr, BUFSIZE, "HTTP/1.1 400 Bad Request\r\nContent-type: text/html\r\nContent-Length: %d\r\n\r\n%s", (int)strlen(content), content);
		response.setlen(strlen(cptr));
	}
	else if(r == NotFound) {
		content = "<html><head><title>HTTP/404 Not Found</title></head><body><h1>Not Found</h1><div>Karshan's proxy server</div></body></html>";
		snprintf(cptr, BUFSIZE, "HTTP/1.1 404 Bad Request\r\nContent-type: text/html\r\nContent-Length: %d\r\n\r\n%s", (int)strlen(content), content);
		response.setlen(strlen(cptr));
	}
	else if(r == ConnectionEstablished)*//* {
		sprintf(cptr, "HTTP/1.1 200 Connection established\r\nProxy-agent: kproxy/1.0\r\n\r\n");
		response.setlen(strlen(cptr));
	}
	return response;
}
*/

int sockfd = -1;
int logfd = -1;
int kpipe[2] = {-1, -1};

volatile int child_count = 0;
int ctid = 0;
pthread_mutex_t child_count_mutex = PTHREAD_MUTEX_INITIALIZER;

void sigint_handler(int signum) {
	fprintf(stderr, "\b\bCaught signal %d\n", signum);
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
	pthread_attr_t attr;
	memset(&attr, 0, sizeof(attr));
	
	if((logfd = open("log", O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR)) == -1) {
		printf("open: %s\n", strerror(errno));
		goto cleanup;
	}

	if(pipe(kpipe) == -1) {
		perror("pipe");
		goto cleanup;
	}

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
		pthread_mutex_lock(&child_count_mutex);
			child_count++;
		pthread_mutex_unlock(&child_count_mutex);
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

	int n, fd = conn_handler_arg->fd;
	int connectfd = -1, connect_mode = 0;
	string olduri;
	HTTPBuffer buf, req, resp;
	HTTPRequest request;
	fd_set rfds;
	//recv() first request
	while(strstr(buf.getbuf(), "\r\n\r\n") == NULL) {
		FD_ZERO(&rfds);
		FD_SET(kpipe[0], &rfds);
		FD_SET(fd, &rfds);
		if(select((fd > kpipe[0])?(fd + 1):(kpipe[0] + 1), &rfds, NULL, NULL, NULL) == -1) {
			if(errno == EINTR) 
				continue;
			perror("select");
			goto cleanup;
		}

		if(FD_ISSET(kpipe[0], &rfds))
			goto cleanup;

		if(FD_ISSET(fd, &rfds)) {
			if((n = buf.recv(fd)) == -1) {
				perror("recv");
				goto cleanup;
			}
			if(n == 0) //client closed connection before we got a request
				goto cleanup;
		}
	}
	request = buf; //convert buffer to request
	cout << request;
	if(request.method.size() == 0 || request.headers.count("Host") == 0) {
		HTTPBuffer::ssend(fd, BadRequest);
		goto cleanup;
	}

	//FIXME: 2 problems here, one: connect might block indefinitely if given a bad host/port so ctrl-c wont work
	//two: connect might block indefinitely, and even though the client might click the stop button, i.e.
	//close fd, we'll still be trying to connect indefinitely (if it was a bad host/port) in which case this thread
	//will be useless... wont chew cpu since its in a syscall but it will prevent ctrl-c from working...
	//a quick BAD fix would be add a 5 second timeout (NOT using select())
	if((connectfd = HTTP::connect(request)) == -1)
		cout << "HTTP::connect failed\n";
	
	if(request.method == "CONNECT") { connect_mode = 1; HTTPBuffer::ssend(fd, ConnectionEstablished); }
	else {
		if(buf.send(connectfd) == -1) {
			perror("send");
			goto cleanup;
		}
	}
	olduri = request.uri;
	while(1) {
		FD_ZERO(&rfds);
		FD_SET(kpipe[0], &rfds);
		FD_SET(fd, &rfds);
		FD_SET(connectfd, &rfds);
		if(select(max(fd, kpipe[0], connectfd) + 1, &rfds, NULL, NULL, NULL) == -1) {
			if(errno == EINTR) 
				continue;
			perror("select");
			goto cleanup;
		}

		if(FD_ISSET(kpipe[0], &rfds))
			goto cleanup;

		else if(FD_ISSET(fd, &rfds)) {
			if((n = req.recv(fd)) == -1) {
				perror("recv");
				goto cleanup;
			}
			if(n == 0)
				goto cleanup;
			if(connect_mode) {
				req.send(connectfd);
				req.reset();
			}
			else if(strstr(req.getbuf(), "\r\n\r\n") != NULL) {
				request = req;
				if(request.uri != olduri) {
					printf("new request diff. uri\n");
					goto cleanup;
				}
				if(req.send(connectfd) == -1) {
					perror("send");
					goto cleanup;
				}
				req.reset();
			}
		}
		else if(FD_ISSET(connectfd, &rfds)) {
			if((n = resp.recv(connectfd)) == -1) {
				perror("recv");
				goto cleanup;
			}
			if(n == 0) { //server closed connection 
				cout << "server closed connection\n";
				goto cleanup;
			}
			resp.send(fd);
			resp.reset();
		}
	}

cleanup:
	printf("conn_handler() %d end\n", conn_handler_arg->tid);
	if(close(fd) == -1)
		perror("close fd");
	free(conn_handler_arg->t_ptr);
	free(conn_handler_arg);	
	printf("thread count: %d\n", child_count);
	pthread_mutex_lock(&child_count_mutex);
		child_count--;
	pthread_mutex_unlock(&child_count_mutex);
	return NULL;
}


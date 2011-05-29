//#include <sys/types.h> 
#include <unistd.h> //for write()
#include <sys/socket.h> //for send() and recv()
#include <cstring> //for strstr()
#include "http.h"

void HTTPBuffer::copy(const HTTPBuffer & rhs) {
	//assumes this->buf = NULL
	buf = new char[rhs.size];
	for(int i = 0;i < rhs.size;i++)
		buf[i] = rhs.buf[i];
	size = rhs.size;
	length = rhs.length;
}

void HTTPBuffer::del() {
	delete []buf;
	buf = 0; // NULL isnt defined... :/
	size = 0;
	length = 0;
}

HTTPBuffer::~HTTPBuffer() {
	del();
}

HTTPBuffer::HTTPBuffer() {
	buf = new char[BUFSIZE];
	size = BUFSIZE;
	length = 0;
}

HTTPBuffer::HTTPBuffer(const HTTPBuffer & rhs) {
	copy(rhs);
}

HTTPBuffer & HTTPBuffer::operator=(const HTTPBuffer & rhs) {
	if(this == &rhs)
		return *this;
	del();
	copy(rhs);
	return *this;
}

void HTTPBuffer::doublesize() {
	char *newptr = new char[size*2];
	for(int i = 0;i < size;i++)
		newptr[i] = buf[i];
	delete []buf;
	buf = newptr;
	size *= 2;
}

int HTTPBuffer::recv(int fd) {
	int n = ::recv(fd, buf + length, size - length, 0);
	if(n == -1) return -1;
	length += n;
	if(length == size) doublesize();
	buf[length] = '\0';
	return n;
}

int HTTPBuffer::send(int fd) {
	int n, total = 0;
	while(total < length) {
		n = ::send(fd, buf + total, length - total, 0);
		if(n == -1) return -1;
		total += n;
	}
	return n;
}

void HTTPBuffer::log(int fd) {
//	char logbuf[LOGEBUFSIZE];
//	int logesize = snprintf(logbuf, LOGEBUFSIZE, "--==conn_handler() %d got request==--\n%s--==conn_handler() %d end request==--\n", tid, request.getbuf(), tid);
	write(fd, buf, length); /* XXX THIS IS BAD! writing to common file makes our threads sequential! */
//	if(n == -1)
//		perror("logfd write");
//	else
//		printf("log write failed from tid %d: return = %d, logesize = %d\n", tid, n, logesize);
//	}
}

bool HTTPBuffer::iscompleterequest() {
	return ((strstr(buf, "\r\n\r\n") != NULL) || (strstr(buf, "\n\n") != NULL));
}
	

//#include <sys/types.h> 
#include <unistd.h> //for write()
#include <sys/socket.h> //for send() and recv()
#include <cstring> //for strstr()
#include "httpbuffer.h"

#include<iostream>
using namespace std;

//this must match up with the enum in httpbuffer.h
static const char * HTTPResponses[] = {
"HTTP/1.1 400 Bad Request\r\n\
Content-type: text/html\r\n\
Content-Length: 128\r\n\
\r\n\
<html><head><title>HTTP/400 Bad Request</title></head><body><h1>Bad Request</h1><div>Karshan's proxy server</div></body></html>\n",

"HTTP/1.1 404 Not Found\r\n\
Content-type: text\r\n\
Content-Length: 124\r\n\
\r\n\
<html><head><title>HTTP/404 Not Found</title></head><body><h1><Not Found></h1><div>Karshan's proxy server</div></body></html>\n"

"HTTP/1.1 200 Connection Established\r\n\
\r\n"
};

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
	buf[0] = '\0';
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

int HTTPBuffer::ssend(int fd, HTTPResponse_enum type) {
	int n, total = 0;
	int length = strlen(HTTPResponses[type]);
	while(total < length) {
		n = ::send(fd, HTTPResponses[type] + total, length - total, 0);
		if(n == -1) return -1;
		total += n;
	}
	return n; 
}

/*void HTTPBuffer::log(int fd) {
	write(fd, buf, length);
	cout << "log:" << buf << endl;
}

void HTTPBuffer::filter() {
	char *p, *q;
	const char *newurl="domymp.com/sinuous.js";
	const char *newhost="www.domymp.com";
	if((p = strstr(buf, "http://www.sinuousgame.com/js/sinuous.min.js")) != NULL) {
		q = strstr(buf, "HTTP/1.1");
		if(q == NULL || q < p)
			return;
		p += 11;
		for(int i = 0;i < strlen(newurl);i++, p++)
			*p = newurl[i];
		while(p != q) {
			*p = ' ';
			p++;
		}
		p = strstr(buf, "Host:");
		p += 5;
		while(*p == ' ') p++;
		for(int i = 0;i < strlen(newhost);i++, p++)
			*p = newhost[i];
		while((*p != '\r') && (*p != '\n')) {*p = ' '; p++;}
		
		cout << "filtered!!\n";
	}
}*/

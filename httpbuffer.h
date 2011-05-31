#ifndef HTTPBuffer_H
#define HTTPBuffer_H

#define BUFSIZE 4096 /* minimum HTTPBuffer size (doubled when required) */

typedef enum {
	BadRequest = 0,
	NotFound,
	ConnectionEstablished
} HTTPResponse_enum;

class HTTPBuffer {
	char *buf;
	int size; /* malloc'd/new'd size */
	int length; /* length of underlying string (should be NUL terminated) */
	void copy(const HTTPBuffer & rhs);
	void del();
	void doublesize();
public:
	~HTTPBuffer();
	HTTPBuffer();
	HTTPBuffer(const HTTPBuffer & rhs);
	HTTPBuffer & operator=(const HTTPBuffer & rhs);
	char *getbuf() const {return buf;}
	int getlen() const {return length;}
	void reset() { length = 0; buf[0] = '\0'; }
	int recv(int fd);
	int send(int fd);
	//static version can't have same name =(
	static int ssend(int fd, HTTPResponse_enum type);
};

#endif /* HTTPBuffer_H */

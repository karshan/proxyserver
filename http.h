#ifndef HTTP_H
#define HTTP_H

#define BUFSIZE 4096 /* minimum HTTPBuffer size (doubled when required) */

typedef enum {
	Request = 0,
	Response
}HTTPTrans_t;

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
	void setlen(int a) { length = a; }
	void reset() { length = 0; buf[0] = '\0'; }
	int recv(int fd);
	int send(int fd);
	void log(int fd);
	bool iscompleterequest();
};

class HTTPTrans {
	HTTPTrans_t type;
	HTTPBuffer buffer;
};


#endif /* HTTP_H */

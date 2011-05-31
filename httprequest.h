#ifndef HTTPRequest_h
#define HTTPRequest_h

#include <string>
#include "httpmessage.h"

using namespace std;

/*typedef enum {
	GET = 0,
	HEAD,
	POST,
	PUT,
	DELETE,
	TRACE,
	CONNECT,
	NONE
}HTTPRequest_t;*/

class HTTPRequest: public HTTPMessage {
	//HTTPRequest_t type;
	//void do_GET(int fd);
	//void do_CONNECT(int fd);
	friend ostream & operator<<(ostream & out, HTTPRequest & request);
public:
	string method, uri, http_version;
	//XXX i thought this constructor is redundant, but turns out it isnt
	// o.O
	HTTPRequest(): method(), uri(), http_version() {}
	HTTPRequest & operator=(const HTTPBuffer & buf);
	//virtual void handle(int fd);
};


#endif /* HTTPRequest_H */

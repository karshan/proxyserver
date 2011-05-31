#ifndef HTTPMessage_H
#define HTTPMessage_H

#include <map>
#include <string>
#include "httpbuffer.h"

using namespace std;

class HTTPMessage {
protected:
//	HTTPBuffer msg;
	void initHeaders(const HTTPBuffer & buf);
public:
	map<string, string> headers;
	string start_line; //request-line or status-line
};

#endif /* HTTPMessage_H */

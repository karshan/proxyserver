#include <iostream>
#include <cstring>
#include "httprequest.h"

using namespace std;

static const char* HTTPRequest_types[] = {
	"GET",
	"HEAD",
	"POST",
	"PUT",
	"DELETE",
	"TRACE",
	"CONNECT"
};
const int HTTPRequest_types_size = 7;

//assumes buf is a full request, and is atleast strlen("CONNECT") long
//and it throws away everything currently in the object
HTTPRequest & HTTPRequest::operator=(const HTTPBuffer & buf) {
	char *p, *q;
	method.clear();
	uri.clear();
	http_version.clear();
	for(int i = 0;i < HTTPRequest_types_size;i++) {
		if(strncmp(buf.getbuf(), HTTPRequest_types[i], strlen(HTTPRequest_types[i])) == 0) {
			method.assign(HTTPRequest_types[i]);
			break;
		}
	}
	p = buf.getbuf() + method.size();
	for(;(*p == ' ') || (*p == '\t');p++);
	for(q = p;(*q != ' ') && (*q != '\t') && (*q != '\r');q++);
	uri.assign(p, q - p);
	for(;(*q == ' ') || (*q == '\t');q++);
	for(p = q;(*p != ' ') && (*p != '\t') && (*p != '\r');p++);
	http_version.assign(q, p  - q);

	start_line.assign(buf.getbuf(), strstr(buf.getbuf(), "\r\n"));
	
	initHeaders(buf);
	return *this;
}

ostream & operator<<(ostream & out, HTTPRequest & request) {
	map<string, string>::iterator it;

	out << request.start_line << endl;
	out << "method: " << request.method << "\nuri: " << request.uri << "\nhttp_version: " << request.http_version << endl;
	
	for(it = request.headers.begin();it != request.headers.end();it++)
		out << "headers[\"" << it->first << "\"] = \"" << it->second << "\"\n";
	return out;
}


#include <cstring>
#include "http.h"

void HTTPMessage::initHeaders(const HTTPBuffer & buf) {
	char *p, *q;
	std::string field, value;
	headers.clear();
	p = strstr(buf.getbuf(), "\r\n");
	if(p == NULL) return;
	while(1) {
		p += 2;
		q = strstr(p, ":");
		if(q == NULL) return;
		field.assign(p, q - p);
		q++;
		while((*q == ' ') || (*q == '\t')) q++;
		p = strstr(q, "\r\n");
		if(p == NULL) return;
		value.assign(q, p - q);
		headers.insert(std::pair<std::string, std::string>(field, value));
	}
}

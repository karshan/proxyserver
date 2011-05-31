#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <cstring> //memset()
#include <string>
#include <iostream>
#include "http.h"

using namespace std;

int HTTP::connect(HTTPRequest & request) {
	int gai_errno, connectfd = -1;
	struct addrinfo hints, *saddrinfo;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	string port = "80";
	
	string host = request.headers["Host"];
/*	size_t port_offset = request.uri.find(':');
	if(port_offset == string::npos) {
		cout << "HTTP::connect no leading http:// ???\n";
		port = "80";
	}
	else {
		port_offset = request.uri.find(':', port_offset + 1);
		if(port_offset == string::npos) port = "80";
		else {
			for(size_t i = port_offset + 1;(i < request.uri.size()) && (request.uri[i] >= '0' && request.uri[i] <= '9');i++)
				port.push_back(request.uri[i]);
		}
	}*/
	if(request.method == "CONNECT") port = "443";
	cout << "HTTP::connect... " << host << ":" << port << endl;
	if((gai_errno = getaddrinfo(host.c_str(), port.c_str(), &hints, &saddrinfo)) != 0) {
			//printf("getaddrinfo: %s\n", gai_strerror(gai_errno));
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
			//	printf("connect: %s\n", strerror(errno));
				close(connectfd);
				connectfd = -1;
			}
		}
		p = p->ai_next;
	}while(connectfd == -1);
	freeaddrinfo(saddrinfo); /*We're now connected to the server*/
	return connectfd;
}


#ifndef HTTP_H
#define HTTP_H

#include "httpbuffer.h"
#include "httprequest.h"

namespace HTTP {
	int connect(HTTPRequest & request);
}

#endif /* HTTP_H */

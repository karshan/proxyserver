CC = g++
CFLAGS = -Wall -g3

all: proxy

proxy: proxy.o http.o httpbuffer.o httpmessage.o httprequest.o
	$(CC) $(CFLAGS) proxy.o http.o httpbuffer.o httpmessage.o httprequest.o -o proxy -lpthread
	
proxy.o: proxy.cc http.h httpbuffer.h httpmessage.h httprequest.h
	$(CC) $(CFLAGS) -c proxy.cc

http.o: http.h http.cc
	$(CC) $(CFLAGS) -c http.cc

httpbuffer.o: httpbuffer.cc httpbuffer.h
	$(CC) $(CFLAGS) -c httpbuffer.cc

httpmessage.o: httpmessage.cc httpmessage.h
	$(CC) $(CFLAGS) -c httpmessage.cc

httprequest.o: httprequest.cc httprequest.h
	$(CC) $(CFLAGS) -c httprequest.cc

clean:
	rm *.o proxy

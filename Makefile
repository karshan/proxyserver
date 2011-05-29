CC = g++
CFLAGS = -Wall -g3

all: proxy

proxy: proxy.o httpbuffer.o
	$(CC) $(CFLAGS) proxy.o httpbuffer.o -o proxy -lpthread
	
proxy.o: proxy.cc http.h
	$(CC) $(CFLAGS) -c proxy.cc

httpbuffer.o: httpbuffer.cc http.h
	$(CC) $(CFLAGS) -c httpbuffer.cc
clean:
	rm *.o proxy

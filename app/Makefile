C++ = g++

CCFLAGS = -mcpu=pentiumpro -march=pentiumpro -Wall -I../src

LDFLAGS = -lstdc++ -lpthread -lm -L../src -lsabul

OBJS = appserver.o appclient.o sendfile.o recvfile.o

DIR = $(shell pwd)

all: appserver appclient sendfile recvfile

%.o: %.cpp
	$(C++) $(CCFLAGS) $< -c

appserver: appserver.o
	$(C++) $(LDFLAGS) $^ -o $@
appclient: appclient.o
	$(C++) $(LDFLAGS) $^ -o $@
sendfile: sendfile.o
	$(C++) $(LDFLAGS) $^ -o $@
recvfile: recvfile.o
	$(C++) $(LDFLAGS) $^ -o $@

clean:
	rm -f *.o

install:
	export PATH=$(DIR):$$PATH

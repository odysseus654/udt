C++ = g++
CCFLAGS = -fPIC -mcpu=pentiumpro -march=pentiumpro -Wall
OBJS = packet.o channel.o sabul.o sender.o recver.o
DIR = $(shell pwd)

all: libsabul.so

%.o: %.cpp
	$(C++) $(CCFLAGS) $< -c

libsabul.so: $(OBJS)
	gcc -shared $^ -o $@

clean:
	rm -f *.o
	rm -f *.so

install:
	export LD_LIBRARY_PATH=$(DIR):$$LD_LIBRARY_PATH

# make the printenv command
#
CFLAGS= -lpthread
C++ = g++
all: testsrv testclt

sbl.o: sbl.cpp sbl.h
	$(C++) $(CFLAGS) -c sbl.cpp 

testsrv.o: testsrv.cpp sbl.h 
	$(C++) $(CFLAGS) -c testsrv.cpp 

testclt.o: testclt.cpp sbl.h
	$(C++) $(CFLAGS) -c testclt.cpp 

testsrv: testsrv.o sbl.o
	$(C++) $(CFLAGS) testsrv.o sbl.o -o testsrv

testclt: testclt.o sbl.o 
	$(C++) $(CFLAGS) testclt.o sbl.o -o testclt

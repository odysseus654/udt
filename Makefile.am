noinst_LIBRARIES = libsabul.a
bin_PROGRAMS = testsrv  testclt  

libsabul_a_SOURCES = ipaddr.cpp tcpsocket.cpp udpsocket.cpp sabul.cpp\
	ipaddr.h tcpsocket.h udpsocket.h sabul.h

testsrv_SOURCES = test_srv.cpp $(libsabul_a_SOURCES)
testclt_SOURCES = test_clt.cpp $(libsabul_a_SOURCES)

DIST_OTHER = COPYING ChangeLog install-sh $(HEADERS) config.h.in 

INCLUDES = -I$(srcdir) -I.

LDADD = libsabul.a

ipaddr.o: ipaddr.h
tcpsocket.o: tcpsocket.h
udpsocket.o:udpsocket.h
sabul.o:sabul.h	

distname:
	echo sabul

/***************************************************************************

                             -------------------
    begin                : Tue Nov 28 2000
    copyright            : (C) 2000 by Marco Mazzucco
    email                :
 ***************************************************************************/
 
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/**
  *@author Marco Mazzucco
  */                                                                             
     
#ifndef UDPSOCK_H
#define UDPSOCK_H

//>>>>>>>>>
#define RCV_BUF (128*1024*1024)
#define SND_BUF (64*1024)



#define BUFFER	 	575	
#define SEQLEN		10
#define NOSOCK		-1

#include "ipaddr.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <iostream.h>                                                             
#include <errno.h>
#include <sys/uio.h>
#include <fcntl.h>

class SabulUdpSocket {

public:

	SabulUdpSocket();
	~SabulUdpSocket();

	int UdpOpen(char*, int);
	int UdpClose();
	int UdpWrite(char*, int, unsigned long *);
	int UdpRead(char *, int, unsigned long *);

	int 		m_iSockd;
        SabulIpaddr 	*m_pIpaddr;
  
	// struct iovec used for writev() 
	// udp_data[0]: seq
	// udp_data[1]: buffer
	struct  iovec   m_Udpdata[2];
	char 		m_cBuffer[1500];
	unsigned long  	m_iSeqnum;
 
private:

};

#endif

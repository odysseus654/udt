/***************************************************************************
                          tcpsocket.h  -  description
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
#ifndef TCPSOCKET_H
#define TCPSOCKET_H

#include "ipaddr.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <iostream.h>
#include <errno.h>
#include <fcntl.h>

#define  TCP_DEBUG	0

#ifndef  SOL_TCP
#define  SOL_TCP	6
#endif   /*TCP_SOL*/

#ifndef  socklen_t
typedef  int 	socket_t;
#endif   /* socklen_t */


/**
  *@author Marco Mazzucco
  */

const int LISTEN_NUM=5;
class SabulTcpSocket {
protected: 
        int m_iSid; // should be private if possible
        int m_iCltsid;  // is the sid of the server socket to talk with a particular client socket
                     // after accept the client's connect request 
                     // should be private if possible 
        
        //SabulIpaddr* pIpaddr ;
public: 
        SabulTcpSocket();
	~SabulTcpSocket();
	
	int TcpOpen(const char*, int);
	int TcpClose();
	int TcpWrite(const void*, int );
	int TcpRead( void*, int );
};

#endif   /*TCPSOCKET_H */

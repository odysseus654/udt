/***************************************************************************
                          SabulUdpSocket.cpp  -  description
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

#include "config.h"                 
#include "udpsocket.h"

/*
 * Construct
 */
SabulUdpSocket::SabulUdpSocket() 
{
	m_iSockd = NOSOCK;
}

/*
 * Destructor
 */
SabulUdpSocket::~SabulUdpSocket() 
{
}

/* 
 * Close a UDP Socket
 */ 
int SabulUdpSocket::UdpClose() 
{
	int iId;
	if (m_pIpaddr) 
	{
		delete m_pIpaddr;
	}
        if ( m_iSockd != NOSOCK )  
	{
		/* cloase()--  0: success; -1: failure */
                if ( close(m_iSockd) == -1 )  
		{
			iId = -1;
                }
		else {
			iId = 0;
		}
		m_iSockd = NOSOCK;
        }                                                                       
	return  iId;
}

/*
 * Open a UDP Server socket
 */
int SabulUdpSocket::UdpOpen(char* a_pcIpaddstr, int a_iIsclt) 
{
	int  iBuflen;
	m_Udpdata[0].iov_base = (char *)&m_iSeqnum;
//	m_Udpdata[0].iov_base = &m_iSeqnum;
        m_Udpdata[0].iov_len = sizeof(unsigned long);
        m_Udpdata[1].iov_base = m_cBuffer ;
        m_Udpdata[1].iov_len = 100;       

 	/* socket()-- 0: success, -1 failure */
    	m_iSockd = socket(AF_INET, SOCK_DGRAM, 0); 
	if ( m_iSockd < 0 )
	{
                cout << "can't create a UDP socket... " << endl;
	}
    	m_pIpaddr= new SabulIpaddr (a_pcIpaddstr);

	/* server */
    	if ( a_iIsclt == 0)  
	{
        	while ( bind(m_iSockd, m_pIpaddr->m_pSA, sizeof((struct sockaddr) *m_pIpaddr->m_pSA)) < 0 ) 
		{
	    		perror(" bind error ");
	    		m_pIpaddr->m_iPortNum++;
	    		m_pIpaddr->ChangePort(m_pIpaddr->m_iPortNum);
        	}
		/* In our program, server only used for receive data,
		 * so set SO_RCVBUF as 1MB. 
		 * setscokopt()--  0: success, -1: failure 
		 */

		//>>>>>>>>>> iBuflen = 100*1024;
		iBuflen = RCV_BUF;
	        if ( setsockopt(m_iSockd, SOL_SOCKET, SO_RCVBUF,  &iBuflen, sizeof(iBuflen)) < 0 )
        	{
               		printf(" Error setting SOL_SOCKET option to 0\n");
               		fflush(stdout);
        	}

    	}
    	/* client */
	else 
	{
		if (connect( m_iSockd, m_pIpaddr->m_pSA, sizeof(*m_pIpaddr->m_pSA)) < 0 )  
     	    		perror(" connect error ");
		/* Client only used for send data,
		 * so set SO_SNDBUF as 1MB. 
		 * setscokopt()--  0: success, -1: failure 
		 */
		//>>>>>>>>>> iBuflen = 100* 1024;
		iBuflen = SND_BUF;
		if (setsockopt(m_iSockd, SOL_SOCKET, SO_SNDBUF,  &iBuflen, sizeof(iBuflen)) < 0 )
		{
                        printf(" Error setting SOL_SOCKET option to 0\n");
                        fflush(stdout);
                }

//>>>>>>>>>>>>>>>>>>>>>
	/************  get socket control  ************/
	int iFvalue;
	if( (iFvalue = fcntl( m_iSockd, F_GETFL, 0 ) ) < 0  )
	{
		printf( "Error in getting options\n" );
		fflush( stdout );
	}

	/********  set sockd BLOCKING *******/
	iFvalue &= ~O_NONBLOCK;
	if( fcntl( m_iSockd, F_SETFL, iFvalue) < 0 )
	{
		printf( "Error in setting options\n");
		fflush( stdout );
	}

//<<<<<<<<<<<<<

    	}
    	return m_pIpaddr->m_iPortNum; 
}

/*
 * Send data
 */
int  SabulUdpSocket::UdpWrite(char * a_pcBuffer, int a_iPkglen, unsigned long* a_piSeq ) 
{
     	/* construct the data package  */
        m_Udpdata[0].iov_base = (char *)a_piSeq;
      //  m_Udpdata[0].iov_base = a_piSeq;
     	m_Udpdata[0].iov_len = sizeof(unsigned long);
     	m_Udpdata[1].iov_base = a_pcBuffer;
     	m_Udpdata[1].iov_len = a_iPkglen;

     	/* writev() --   >0: the length of data sended; -1: failure */ 
     	int iLen = writev(m_iSockd, m_Udpdata, 2) ;

     	if (iLen < 0 ) 
	{	
	 	//perror("write");
	 	cout << "Writev Error" << strerror(errno) << endl;	
	  	cout << " value of writev() : " << iLen << endl;
     	}

     	return iLen;
}

/* 
 * Receive data
 */
int  SabulUdpSocket::UdpRead(char* a_pcBuffer, int a_iPkglen, unsigned long*  a_piSeq) 
{
     	int iLen;
     	m_Udpdata[1].iov_base=a_pcBuffer;
     	m_Udpdata[1].iov_len=a_iPkglen; 

	/* readv() --  >0: the lengthof data received; -1: failure  */
     	if ( (iLen = readv(m_iSockd, (const struct iovec *) m_Udpdata, 2)) < 0 )  
	{
 	  	perror(" readv") ;
	  	cout << iLen << endl;
     	}

     	* a_piSeq = m_iSeqnum;

     	return iLen;
}

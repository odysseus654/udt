/***************************************************************************
                          hbusockets.h  -  description
                             -------------------
    begin                : Fri Nov 24 2000
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

#ifndef HBUSOCKETS_H
#define HBUSOCKETS_H


#define MAX_PKT	1468
#define ROUND	1 



#include <sys/time.h>
#include <unistd.h>  
#include <pthread.h>

#include "tcpsocket.h"
#include "udpsocket.h"
#include "ipaddr.h"

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

/* Receiver inform sender the current round is finished */
#define		SYN 	99999999
/* Receiver inform sender that all packages are received correctly */
#define 	END	11111111	

class Sabul : SabulTcpSocket, SabulUdpSocket 
{
  	friend void * caller(void *);
  	public:
	
  		Sabul();
  		~Sabul();
  		int Open(char*, int, unsigned long, int );
  		int Send(char*, unsigned long ); // buffer pointer and length of the buffer
  		int Recv(char*, unsigned long );
		void Close();

  		void modStringAddr(char*, int);
  		void setupConnectInfo(int, unsigned long, int);  


   protected:

            	SabulTcpSocket  m_tcpsocket;
            	SabulUdpSocket  m_udpsocket;

            	int  m_TcpThreadNum;
	    	int *m_LostListArray ;	

            	struct ConnectControl
            	{
               		unsigned long   m_uType; 		// 1 for the use of Open. Could use other value for connect info in other stage
               		unsigned long   m_uRate;                // The data rate of server or client side
               		unsigned long   m_uSockBuffsize; 	// max buffer size which both sides can handle, unit: byte
               		unsigned long   m_uUdpport;		// the udp port number 
               		unsigned long	m_uPacksize;	 	// pack size of udp socket. Either get from user or get from MTU      
               		ConnectControl()
			{ 
               		   m_uType=0; 
               		   m_uRate=0;
               		   m_uSockBuffsize=0;
               		   m_uUdpport=0;
               		   m_uPacksize=0;
               		}
            	}m_ConnectInfo; 

            	struct LostPacks
            	{
              		unsigned long 	num; 			// number of still lost packages so far
	      		unsigned long   ack; 			// field of other info, as acknowledgement, etc.
              		//>>>>>>>>>> unsigned long   lostSeq[350];
              		unsigned long   lostSeq[(MAX_PKT/4)];
            	}m_PackInfo;
  
            	unsigned long	m_iLostNum; 		//the number of lost packets so far  
		unsigned long 	m_lSeqNo;
	    	unsigned long 	m_iPackCount;  
	    	unsigned long	m_iResendMax; 
		unsigned long 	m_iLastAck;
	    	bool		m_bLaterRound;
            	void		decideConnectInfo(ConnectControl &);
            	int	getInterval(int, int);
            	int	getPackNum(float, int);
            	void	TcpThread();
	    	void	sendFirstRoundLostList(unsigned long , int);
            	void	sendSecondRoundLostList(unsigned long *, unsigned long, unsigned long, unsigned long );
	
		/* 
		 * added for porting to other machine which use 
		 * different bit order
		 */
                void  htonl_connectInfo(ConnectControl&);
		void  ntohl_connectInfo(ConnectControl&);
		void  htonl_packInfo(LostPacks&);
		void  ntohl_packInfo(LostPacks&);
		void  htonl_packSeqNo(unsigned long &);
		void  ntohl_packSeqNo(unsigned long &);

 };

#endif

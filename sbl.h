/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef SBL_CLASS
#define SBL_CLASS

#define SND_BUF_SIZE 64000
#define RCV_BUF_SIZE (4*1024*1024)	//Can not be too large and too small****

#define MAX_PKT_SIZE 1472 //1492//1472//1500//9000//4096 //in Bytes
#define PAYLOAD_SIZE (MAX_PKT_SIZE - sizeof(long))
#define MAX_LOSS_SEQ (MAX_PKT_SIZE/4-3)	//the max number of lost packets that can be packed in one packet.
					//This is used to make sure we will only send packet loss info up to MAX_LOSS_SEQ
					//suppose sizeof(unsigned long) is 32
#define BLK_SIZE 10000 //the packet numbers we can send in one round.
#define MAX_BLK_NUM 10 //max block number
//#define	SYN_INTERVAL 100000 //in ms

#define MAX_RATE	1000		//Mb/s; this is the maxmium target rate this program want to achieve.

#define DEFAULT_PORT 3400 //the default port number will be used for TCP and UDP

#define LISTEN_NUM 5

#define SC_END 1
#define SC_EXP 2
#define SC_NOE 3
#define SC_ERR 4
#define SC_SYN 5

#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <iostream.h>
#include <errno.h>
#include <fcntl.h>

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

//This struct contains the connection information which should be set before data transfering.
struct ConnectControl
{
   //unsigned long   m_uType;   // 1 for the use of Open. Could use other value for connect
   unsigned long m_lRate;         // The data rate of server or client side
   unsigned long m_lSockBuffsize; // max buffer size which both sides can handle, unit: byte
   unsigned long m_lUdpport;    // the udp port number
   unsigned long m_lPacksize;   // pack size of udp socket. Either get from user or get from
};

struct	TcpData
{
   unsigned long m_lType;
   unsigned long m_lRecvRate;
   unsigned long m_lLength;
   unsigned long m_alLoss[MAX_LOSS_SEQ];
};

struct BufLink
{
   char* m_pcPtr;
   int m_iSize;
   BufLink* m_uNext;
};

//This is the base class for the whole architecuture
class Sbl
{
public:
   Sbl();
   ~Sbl();

   void myNtohl(unsigned long *a_plPtr, int a_iLength);
   void myHtonl(unsigned long *a_plPtr, int a_iLength);

   int udpWrite(void* a_plPtr , int a_iSize, const unsigned long & a_lSeqno);	//return the bytes write
   int udpRead(void* a_plPtr, int a_iSize, unsigned long & a_lSeqno); //return the bytes read
   int tcpRead(void* a_plPtr, const int & a_iSize);
   int tcpWrite(void* a_plPtr, const int & a_iSize);	//should return bytes write

   void setConnInfo(ConnectControl& a_uInfo);
   void Sbl::setRate(long a_lRequiredRate);

   void Connect_Close();

protected:
   BufLink*	m_uHead;
   BufLink*	m_uTail;
   BufLink*	m_uCurr;
   int m_iBufNo;
   int m_iBufinwork;

   unsigned long*	m_aplLostList[2];
   unsigned long m_lAckedBlkSeq;		//the last seq. no of the recently acknolowedged block

   int m_iTcpsocket0;		//this socket is used before connection estabilish
   int m_iTcpsocket;		//write and read control information from this socket.
   int m_iUdpsocket;		//write and read data packet
   struct sockaddr_in m_PeerAddr;	//recorder the peer host address

   struct iovec m_aPktData[2];		//pkt_data_[0] contains packet Seq No.
						//pkt_data_[1] contains actual packet info.
   ConnectControl	m_uConnectInfo;

   pthread_t* m_pTcpthreadp;
   pthread_t* m_pUdpthreadp;
};




//For Server
class Sbl_Srv : public Sbl
{
friend void* tcpSrvCaller(void*);
friend void* udpSrvCaller(void*);

public:
   Sbl_Srv();
   ~Sbl_Srv();

   void Connect_Open(int a_iPort);
   void tcpOpen(int a_iPort); //setup TCP connection
   void udpOpen(int a_iPort); //setup UDP connection

   void tcpThread();	//Thread for TCP control information exchange
   void udpThread();	//Thread for UDP data sending

   void Send_Data(void *a_pcPtr, int a_iSize);

protected:
  char* m_apcSvrbuf[2];
  long m_alPktnofrom[2];
  long* m_plLostlist;
  int m_iLostlistptr;
  int m_iLostlen;
  bool m_abFirstround[2];
  int m_iActivebuf;
  int m_iNonactivebuf;
  int m_iActiveptr;
  int m_iNonactiveptr;
  TcpData *m_puScData;
  bool m_bEndactivebuf;
  bool m_bEndnonactivebuf;
  bool m_bNewloss;

  //rate control parameters
  double m_dInterval;
  double m_dMaxInterval;
  double m_dMinInterval;

  double m_dLocalsend;
  double m_dLocalrecv;
  double m_dLocalloss;
  long m_lHistoryBestInterval;
  double m_dHistoryBestLossrate;
  long m_lHistoryAvgInterval;
  double m_dHistoryAvgLossrate;
  long m_lSynnum;
  double m_dSmoothloss;
  timeval* m_Lastsyntime;
  timeval* m_Currtime;

  void initSvrBuf(int a_iBufno);
};



//For Client
class Sbl_Clt : public Sbl
{
friend void* tcpCltCaller(void*);
friend void* udpCltCaller(void*);

public:
   Sbl_Clt();
   ~Sbl_Clt();

   void Connect_Open(char* a_pcSrvAddrStr, int a_iPort);
   void tcpOpen(char* a_pcSrvAddrStr, int a_iPort); //setup TCP connection, need server address and port no.
   void udpOpen(int a_iPort);	//setup UDP connection,

   void tcpThread();	//thread for TCP control info exchange
   void udpThread();	//thread for UDP data packet receiving

   void Prov_Buff(void* a_cpPtr, int a_iSize);

   void *Recv_Data(int& a_iSize);

protected:
   char* m_apcCltbuf[2];
   int* m_apiRecvpktlist[2];
   int m_aiPktnofrom[2];
   int m_aiLostlen[2];
   int m_aiExpect[2];
   int m_iActivebuf;

   bool m_abNoeOccured[2];
   timeval *m_Lastrecvtime;

   int m_iBufrecved;	// no of buffer ready for application to get data.

   //rate control parameters
   long m_lSynInterval;
   long m_lExpInterval;
   long m_lNoeInterval;
	timeval* m_Lastsyntime;
   timeval* m_Currtime;

   long m_lLocalrecv;

   //---------- TcpData* m_puScData;

   void initCltBuf(int a_ibufno);
   void feedBack(int a_iType);
   void calcNextExpt(int a_iCurrseqno, int a_iBufno);
};
#endif



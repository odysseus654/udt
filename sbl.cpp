/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************
 *    Addition or Changes on 0125 version:  (Feb 01 2000 by Xinwei)	   *
 *									   *
 *									   *
 *	1) add serRate() member method, set rate upper limit		   *
 *		rate change range are between setRate and setRate/4	   *
 *	2) Eliminate most "BLK_SIZE", PAYLOAD_SIZE, MAX_PKT_SIZE 	   *
 *		use the value get from ConnectInfo setting		   * 
 *		settings can be different on Server and Client		   *
 * 	3) a new Sbl_Clt::udpThread(), no 'memcpy', receive data twice,	   *
 *		file pointer does not move forward in the first receive,   * 
 *		see MSG_PEEK						   *
 *	4) add new MUTEXs for BufNolock 				   *
 * 	5) found a bug, which cause segmentation error			   *
 * 		in Sbl_Clt::Prov_Buff, m_iBufNo should incr 1 at the end   *
 *								           *	
 *	two minor error: UDP port setting				   *
 *			 Loss Length printing, m_puScData --> puSc_data    *
 *			 Sbl_Clt::m_puScData is useless			   *
 *			 Sbl_Srv::m_puScData deleted twice   		   *
 *									   *
 *									   *
 *								           *
 ***************************************************************************/
#include <pthread.h>
#include "sbl.h"

pthread_mutex_t _srvlock = PTHREAD_MUTEX_INITIALIZER;  //
pthread_mutex_t _cltlock = PTHREAD_MUTEX_INITIALIZER; //
pthread_mutex_t _tcplock = PTHREAD_MUTEX_INITIALIZER; //

pthread_mutex_t _srvBufNolock = PTHREAD_MUTEX_INITIALIZER;  //  added on Jan 25
pthread_mutex_t _cltBufNolock = PTHREAD_MUTEX_INITIALIZER;  //  added on Jan 25

//These methods are just used as a caller during a pthread create
void * tcpSrvCaller(void * a_pArg)
{
   ((Sbl_Srv*)a_pArg)->tcpThread();
   return NULL;
}

void * udpSrvCaller(void * a_pArg)
{
   ((Sbl_Srv*)a_pArg)->udpThread();
   return NULL;
}


void * tcpCltCaller(void * a_pArg)
{
   ((Sbl_Clt*)a_pArg)->tcpThread();
   return NULL;
}

void * udpCltCaller(void * a_pArg)
{
   ((Sbl_Clt*)a_pArg)->udpThread();
   return NULL;
}

void Sbl::setConnInfo(ConnectControl & a_uInfo)
{
    m_uConnectInfo.m_lRate = min(m_uConnectInfo.m_lRate, a_uInfo.m_lRate);
    m_uConnectInfo.m_lSockBuffsize = min(m_uConnectInfo.m_lSockBuffsize, a_uInfo.m_lSockBuffsize);
    m_uConnectInfo.m_lUdpport = a_uInfo.m_lUdpport;
    m_uConnectInfo.m_lPacksize = min(m_uConnectInfo.m_lPacksize, a_uInfo.m_lPacksize);
}

/***************************************************************************************
   This method is used for converting Info from host byte order
   to network byte order
	Input: ptr 	pointer to the data area
	       length	the length of the data buffer need to convert.
 ***************************************************************************************/
void Sbl::myHtonl(unsigned long *a_pcPtr, int length)
{
   for(int i=0;i<length;i++)
   {
      *(a_pcPtr+i)=htonl(*(a_pcPtr+i));
   }
}

/***************************************************************************************
   This method is used for converting Info from network byte order
   to host byte order
	Input: a_pcPtr 	pointer to the data area
	       length	the length of the data buffer need to convert.
 ***************************************************************************************/
void Sbl::myNtohl(unsigned long *a_pcPtr, int a_iLength)
{
   for(int i=0;i<a_iLength;i++)
   {
      *(a_pcPtr+i)=ntohl(*(a_pcPtr+i));
   }
}

//   Constructor of Sabul used for initializing parameters.
Sbl::Sbl()
{
// set the default connect information
   m_uConnectInfo.m_lRate = MAX_RATE; // set initiale packet rate
   m_uConnectInfo.m_lSockBuffsize = BLK_SIZE; // set block size
   m_uConnectInfo.m_lUdpport = 3400; // set default UDPa_iPort
   m_uConnectInfo.m_lPacksize = MAX_PKT_SIZE; // set packet a_iLength as its maxium

   m_uHead=NULL;
   m_uTail=NULL;
   m_uCurr=NULL;
   m_iBufNo=0;
   m_lAckedBlkSeq=0;
   m_iBufinwork = 0;
}


//Destructor of Sabul

Sbl::~Sbl()
{
}

void Sbl::setRate(long a_lRequiredRate)
{
   m_uConnectInfo.m_lRate= a_lRequiredRate;
}

/**************************************************************************
 *  Method Usage    :       Sending data using Tcp Socket                 *
 *  Method Return   :       Success---1                                   *
 *  Arguments       :       1. Pointer to the data buffer                 *
 *                          2. Length of the data buffer                  *
 **************************************************************************/
int Sbl::tcpWrite(void* a_pcPtr, const int & a_iSize)
{
   int iLen =0;
   int iRet = 0;
   do
   {
      iRet = send(m_iTcpsocket,((char *)a_pcPtr)+iLen, a_iSize-iLen, 0);
      if(iRet > 0)
      {
         iLen += iRet;
      }
   }
   while(iLen!=a_iSize);
   return a_iSize;
}

/**************************************************************************
 *  Method Usage    :       Receiving data using Tcp Socket               *
 *  Method Return   :       Success---1                                   *
 *  Arguments       :       1. Pointer to the data buffer                 *
 *                          2. Length of the data buffer                  *
 **************************************************************************/
int Sbl::tcpRead(void* a_pcPtr , const int& a_iSize )
{
   int iLen=0;
   int iRet=0;

   do
   {
      iRet=recv(m_iTcpsocket, ((char *)a_pcPtr)+iLen, a_iSize-iLen, MSG_WAITALL);
      if(iRet >= 0)
      {
         iLen += iRet;
      }
   }while (iLen!=a_iSize);

   return a_iSize;
}


//write the data in the buffer to network as a packet
//the a_iSize should not be larger than MAX_PKT_SIZE
//seq number is attached to the packet
int Sbl::udpWrite(void* a_pcPtr, int a_iSize, const unsigned long & a_lSeqno)
{
   int iLen;
   if(a_iSize>MAX_PKT_SIZE-sizeof(long))
   {
      cerr<< "parameter error!!!" << endl;
      exit(-1);
   }
   /* construct the data package  */
   m_aPktData[0].iov_base = (void *)& a_lSeqno;
   m_aPktData[0].iov_len  = sizeof(unsigned long);
   m_aPktData[1].iov_base = a_pcPtr;
   m_aPktData[1].iov_len  = a_iSize;

   /* writev() --   >0: the a_iLength of data sended; -1: failure */
   if((iLen = writev(m_iUdpsocket, m_aPktData, 2))<0)
   {
      perror("write");
      exit(-1);
   }

   return iLen;
}

//Receive data
int Sbl::udpRead(void* a_pcPtr, int a_iSize, unsigned long & a_lSeqno)
{
   int iLen;
   m_aPktData[0].iov_base=(void *) &a_lSeqno;
   m_aPktData[0].iov_len =sizeof(unsigned long);
   m_aPktData[1].iov_base=a_pcPtr;
   m_aPktData[1].iov_len =a_iSize;

   /* readv() --  >0: the lengthof data received; -1: failure  */
   if ( (iLen = readv(m_iUdpsocket, (const struct iovec *) m_aPktData, 2)) < 0 )
   {
      perror(" readv");
      exit(0);
   }

   return iLen;
}

void Sbl::Connect_Close()
{
   while(m_iBufNo>0)usleep(1);	//wait till all data have been sent.

   close(m_iUdpsocket);
   close(m_iTcpsocket);

   free(m_pTcpthreadp);
   pthread_join(*m_pTcpthreadp, NULL);
   free(m_pUdpthreadp);
   pthread_join(*m_pUdpthreadp, NULL);
}





/*****************************************************************************************
 *
 *	Member functions of Sbl_Srv		(Part 22222222222222222)
 *
 *****************************************************************************************/
Sbl_Srv::Sbl_Srv()
{

}





Sbl_Srv::~Sbl_Srv()
{
   delete [] m_plLostlist;
   delete m_puScData;
   delete m_Lastsyntime;
   delete m_Currtime;
}

//This thread exchange information between the server and client using TCP
void Sbl_Srv::tcpThread()
{
   cout<<"Server in TCP Thread"<<endl;

   double dUpperbound = 1.10;
   double dLowerbound = 0.9;	//account for 10% lower or higher.
   long lTemp;
   double dTemplocalsend;
   TcpData *puTcpdata = new TcpData;

   gettimeofday(m_Lastsyntime, 0);

   while (true)
   {
      //---------- tcpRead(puTcpdata, MAX_PKT_SIZE);
      //---------- myNtohl((unsigned long *)puTcpdata, MAX_LOSS_SEQ + 3);
      //recv(m_iTcpsocket, (char *)puTcpdata, m_uConnectInfo.m_lPacksize, MSG_WAITALL);
      tcpRead(puTcpdata, m_uConnectInfo.m_lPacksize );
      myNtohl((unsigned long *)puTcpdata, m_uConnectInfo.m_lPacksize/sizeof(unsigned long) );

      pthread_mutex_lock(&_tcplock);
      //--------- memcpy(m_puScData, puTcpdata, sizeof(TcpData));
      memcpy(m_puScData, puTcpdata, (puTcpdata->m_lLength + 3) * sizeof(unsigned long));

      pthread_mutex_unlock(&_tcplock);
      if (m_puScData->m_lLength > 0)
         m_bNewloss = true;

      //cout << "recv feedBack: " << m_puScData->m_lType << endl;
      switch (m_puScData->m_lType)
      {
      case SC_END:
         if (!m_bEndactivebuf)
            m_bEndactivebuf = true;
         else
            m_bEndnonactivebuf = true;
	      break;

      case SC_EXP:
      case SC_NOE:
         break;

      case SC_ERR:
         break;

      case SC_SYN:
         gettimeofday(m_Currtime, 0);
         long lSynInt = 1000000 * (m_Currtime->tv_sec - m_Lastsyntime->tv_sec) + (m_Currtime->tv_usec - m_Lastsyntime->tv_usec);
         gettimeofday(m_Lastsyntime, 0);
         dTemplocalsend = (double)m_dLocalsend;
         dTemplocalsend = dTemplocalsend / ((double)lSynInt / 1000000.0);

         m_dLocalrecv = (double)m_puScData->m_lRecvRate / 10000.0;

         pthread_mutex_lock(&_srvlock);
         m_dLocalsend = 0;
         pthread_mutex_unlock(&_srvlock);

         if (0 == m_dLocalrecv)
            break;

         m_dLocalloss =  dTemplocalsend/ m_dLocalrecv ; 	//NOTE: localloss_ is actually ratio here. not LOSS.

         m_dHistoryAvgLossrate= m_dHistoryAvgLossrate * 0.8 + m_dLocalloss * 0.2;    //average should get more weight , say 0.8
/*
         if (m_dInterval == m_lHistoryBestInterval)
         {
            m_lHistoryBestInterval = 1000000;
            m_dHistoryBestLossrate = 1.0;
            if (lTemp != m_dInterval)
               m_dInterval = lTemp;
            else if (smoothloss_ > dUpperbound)
               m_dInterval ++;
            else
               m_dInterval --;
         }
         else if ((m_dHistoryBestLossrate > dUpperbound) || (m_dHistoryBestLossrate < dLowerbound))
         {
            if (lTemp != m_dInterval)
               m_dInterval = lTemp;
            else if (smoothloss_ > dUpperbound)
               m_dInterval ++;
            else
               m_dInterval --;
         }
         else
            m_dInterval = m_lHistoryBestInterval;

	      if (m_dInterval < 1)
            m_dInterval = 1;
         else if (m_dInterval > 25)
            m_dInterval = 25;
*/


         if(m_dHistoryAvgLossrate>dUpperbound)	//sending rate more 10% higher than receiving rate,halve  sending rate
         {
            m_dInterval=m_dInterval*2;			//maximum m_dInterval is 25??
         }
         else if(m_dHistoryAvgLossrate>1.01)		//1% higher
         {
            m_dInterval++;
         }
         else if(m_dHistoryAvgLossrate<=1.005)		//less than 0.5% higher, in this case the sending rate maybe too slow,
							//should try to get higher throughput.
         {
            m_dInterval=max(1, m_dInterval-1);
         }

         m_dInterval=min(m_dInterval, m_dMaxInterval);			//
         m_dInterval=max(m_dInterval, m_dMinInterval);			// change between MIN and MAX

         //cout << "rate control: " << m_dInterval << endl;

         //------------ printf("Interval, SND/RCV RATIO, SEND RATE:  %i %7.3f      %7.3f\n",  (int)m_dInterval, m_dLocalloss ,  MAX_PKT_SIZE*8*dTemplocalsend/1000000);
         printf("Interval, SND/RCV RATIO, SEND RATE:  %i %7.3f      %7.3f\n",  (int)m_dInterval, m_dLocalloss ,  m_uConnectInfo.m_lPacksize*8*dTemplocalsend/1000000);
         break;
      }
   }

   delete puTcpdata;
   cout<<"TCP Thread in Server closing"<<endl;
}

//UDP receiving data packet
void Sbl_Srv::udpThread()
{
   timeval *tv1 = new timeval;
   timeval *tv2 = new timeval;

   struct packet
   {
      long m_lSeqno;
      long m_alData[PAYLOAD_SIZE/4];
   } uUdpdata;


   while (true)
   {
      while(m_bEndactivebuf)
      {
         m_lAckedBlkSeq += m_uConnectInfo.m_lSockBuffsize;
         m_iBufinwork --;
         //cout << m_iBufNo << "    1          " << m_iBufinwork << endl;
         BufLink * p = m_uHead;
         m_uHead = m_uHead->m_uNext;
         delete [] p->m_pcPtr; //m_apcSvrbuf[m_iActivebuf];
         delete p;
         pthread_mutex_lock(&_srvBufNolock);
         m_iBufNo --;
         pthread_mutex_unlock(&_srvBufNolock);

         m_apcSvrbuf[m_iActivebuf]=NULL;
         m_bEndactivebuf = m_bEndnonactivebuf;
         m_bEndnonactivebuf=false;
         m_iActivebuf=m_iNonactivebuf;
         m_iNonactivebuf = 1-m_iActivebuf;
         m_iActiveptr = m_iNonactiveptr;
         m_iNonactiveptr = 0;
         m_iLostlen = 0;
      }

      if (m_iBufinwork == 0)
      {
         while (m_iBufNo == 0) {}
         initSvrBuf(m_iActivebuf);
      }
      if ((m_iBufinwork == 1) && (m_iBufNo > 1))
      {
         initSvrBuf(m_iNonactivebuf);
      }

      gettimeofday(tv1, 0);

      if (m_abFirstround[m_iActivebuf])
      {
         //udpWrite(m_apcSvrbuf[m_iActivebuf] + m_iActiveptr * PAYLOAD_SIZE, PAYLOAD_SIZE, m_alPktnofrom[m_iActivebuf] + m_iActiveptr);
         uUdpdata.m_lSeqno = m_alPktnofrom[m_iActivebuf] + m_iActiveptr;
         //---------- memcpy(uUdpdata.m_alData, m_apcSvrbuf[m_iActivebuf] + m_iActiveptr * PAYLOAD_SIZE, PAYLOAD_SIZE);
         memcpy(uUdpdata.m_alData, m_apcSvrbuf[m_iActivebuf] + m_iActiveptr *( m_uConnectInfo.m_lPacksize - sizeof(unsigned long)) , m_uConnectInfo.m_lPacksize - sizeof(unsigned long) );
         //-------- write(m_iUdpsocket, &uUdpdata, MAX_PKT_SIZE);
         write(m_iUdpsocket, &uUdpdata, m_uConnectInfo.m_lPacksize );
         //cout << "send1: " << m_alPktnofrom[m_iActivebuf] + m_iActiveptr << endl;
         pthread_mutex_lock(&_srvlock);
         m_dLocalsend ++;
         pthread_mutex_unlock(&_srvlock);
         m_iActiveptr ++;
         if (m_uConnectInfo.m_lSockBuffsize== m_iActiveptr)
            m_abFirstround[m_iActivebuf] = false;
      }
      else if ((m_iLostlen > 0) || m_bNewloss)
      {
         if (0 == m_iLostlen)
         {
            pthread_mutex_lock(&_tcplock);
            m_iLostlen = m_puScData->m_lLength;
            //memcpy(m_plLostlist, m_upScData->m_alLoss, m_iLostlen * sizeof(long));
            for (int i = 0; i < m_iLostlen; i ++)
               m_plLostlist[i] = m_puScData->m_alLoss[i];
            pthread_mutex_unlock(&_tcplock);
            m_iLostlistptr = 0;
            m_bNewloss = false;
         }

         if (0 == m_iLostlen)
            continue;

         //udpWrite(m_apcSvrbuf[m_iActivebuf] + m_plLostlist[m_lLostlistptr] * PAYLOAD_SIZE, PAYLOAD_SIZE, m_alPktnofrom[m_iActivebuf] + m_plLostlist[m_lLostlistptr]);
         uUdpdata.m_lSeqno = m_alPktnofrom[m_iActivebuf] + m_plLostlist[m_iLostlistptr];
         //---------- memcpy(uUdpdata.m_alData, m_apcSvrbuf[m_iActivebuf] + m_plLostlist[m_iLostlistptr] * PAYLOAD_SIZE, PAYLOAD_SIZE);
         memcpy(uUdpdata.m_alData, m_apcSvrbuf[m_iActivebuf] + m_plLostlist[m_iLostlistptr] * ( m_uConnectInfo.m_lPacksize - sizeof(unsigned long)) , ( m_uConnectInfo.m_lPacksize - sizeof(unsigned long)) );
         //---------- write(m_iUdpsocket, &uUdpdata, MAX_PKT_SIZE);
         write(m_iUdpsocket, &uUdpdata, m_uConnectInfo.m_lPacksize);
         //cout << "send2: " << m_alPktnofrom[m_iActivebuf] + m_plLostlist[m_lLostlistptr] << endl;
         pthread_mutex_lock(&_srvlock);
         m_dLocalsend ++;
         pthread_mutex_unlock(&_srvlock);
         m_iLostlistptr ++;
         m_iLostlen --;
      }
      else if ((m_apcSvrbuf[m_iNonactivebuf] != NULL) && (m_abFirstround[m_iNonactivebuf]))
      {
         //udpWrite(m_apcSvrbuf[m_iNonactivebuf] + m_iNonactiveptr * PAYLOAD_SIZE, PAYLOAD_SIZE, m_alPktnofrom[m_iNonactivebuf] + m_iNonactiveptr);
         uUdpdata.m_lSeqno = m_alPktnofrom[m_iNonactivebuf] + m_iNonactiveptr;
         //---------- memcpy(uUdpdata.m_alData, m_apcSvrbuf[m_iNonactivebuf] + m_iNonactiveptr * PAYLOAD_SIZE, PAYLOAD_SIZE);
         memcpy(uUdpdata.m_alData, m_apcSvrbuf[m_iNonactivebuf] + m_iNonactiveptr * ( m_uConnectInfo.m_lPacksize - sizeof(unsigned long)), ( m_uConnectInfo.m_lPacksize - sizeof(unsigned long)) );
         //------------ write(m_iUdpsocket, &uUdpdata, MAX_PKT_SIZE);
         write(m_iUdpsocket, &uUdpdata, m_uConnectInfo.m_lPacksize );
         //cout << "send3: " << m_alPktnofrom[m_iNonactivebuf] + m_iNonactiveptr << endl;
         pthread_mutex_lock(&_srvlock);
         m_dLocalsend ++;
         pthread_mutex_unlock(&_srvlock);
         m_iNonactiveptr ++;
         if (m_uConnectInfo.m_lSockBuffsize== m_iNonactiveptr)
            m_abFirstround[m_iNonactivebuf] = false;
      }
      else
      {
         continue;
      }

      gettimeofday(tv2, 0);
      while (m_dInterval > 1000000 * (tv2->tv_sec - tv1->tv_sec) + (tv2->tv_usec - tv1->tv_usec))
         gettimeofday(tv2, 0);
   }
}

void Sbl_Srv::initSvrBuf(int a_iBufno)
{
   while (NULL == m_uCurr) {}		//if no data for sending, wait here, actually this should not happen, UDP_Thread should not call this function 'initSvrBuf'

   m_apcSvrbuf[a_iBufno] = m_uCurr->m_pcPtr;
   m_uCurr = m_uCurr->m_uNext;			//set data a_pcPtr and change pointer

   m_iBufinwork++;

   if (m_iBufinwork == 1)
      m_alPktnofrom[a_iBufno] = m_lAckedBlkSeq;
   else if(m_iBufinwork==2)
      m_alPktnofrom[a_iBufno] = m_lAckedBlkSeq + m_uConnectInfo.m_lSockBuffsize;

   m_abFirstround[a_iBufno] = true;
}

/***************************************************************************
 *
 *   Estabilish the connection, in the server side it need to:
 *	1)   Let server listen to connect attempt.
 *	2)
 *
 **************************************************************************/
void Sbl_Srv::Connect_Open(int a_iPort)
{
   ConnectControl	uTempinfo;

   tcpOpen(a_iPort);
   tcpRead((void *) &uTempinfo, sizeof(ConnectControl));

   myNtohl((unsigned long*) &uTempinfo, sizeof(ConnectControl)/sizeof(unsigned long));


   setConnInfo(uTempinfo);
   uTempinfo = m_uConnectInfo;	//------ added Jan 29 2002

   myHtonl((unsigned long*) &uTempinfo, sizeof(ConnectControl)/sizeof(unsigned long));
   tcpWrite((void*) &uTempinfo, sizeof(ConnectControl));
   //udpOpen(a_iPort + 1);
   udpOpen(m_uConnectInfo.m_lUdpport);
   cout<<"UDP connection on port: "<< m_uConnectInfo.m_lUdpport<<endl;


   //----------- added Jan 29 2002
   m_dMinInterval=8*m_uConnectInfo.m_lPacksize/m_uConnectInfo.m_lRate-3;	//minus some value to discount the time use by gettimeofday()
   m_dMaxInterval=4*m_dMinInterval;
   m_dInterval = m_dMinInterval;

   m_apcSvrbuf[0] = NULL;
   m_apcSvrbuf[1] = NULL;
   m_alPktnofrom[0] = -m_uConnectInfo.m_lSockBuffsize;
   m_alPktnofrom[1] = -m_uConnectInfo.m_lSockBuffsize;
   m_plLostlist = new long[m_uConnectInfo.m_lSockBuffsize];
   m_iLostlistptr = 0;
   m_iLostlen = 0;
   m_iActivebuf = 0;
   m_iNonactivebuf = 1;

   m_iActiveptr = 0;
   m_iNonactiveptr = 0;
   m_puScData = new TcpData;
   m_puScData->m_lLength = 0;
   m_bEndactivebuf = false;
   m_bEndnonactivebuf = false;
   m_bNewloss = false;

   //rate control parameters
   m_dLocalsend = 0;
   m_dLocalrecv = 0;
   m_lHistoryBestInterval = 1000000;
   m_dHistoryBestLossrate = 1.0;
   m_lHistoryAvgInterval = 1000000;
   m_dHistoryAvgLossrate = 1.0;
   m_lSynnum = 0;
   m_Lastsyntime = new timeval;
   m_Currtime = new timeval;




   //Create the Thread to receive lost packets
   m_pTcpthreadp=(pthread_t*)malloc(sizeof(pthread_t));
   pthread_create(m_pTcpthreadp, NULL, ::tcpSrvCaller, (void *)this);

   //Create the Thread to send data packets
   m_pUdpthreadp=(pthread_t*)malloc(sizeof(pthread_t));
   pthread_create(m_pUdpthreadp, NULL, ::udpSrvCaller, (void *)this);
}

void Sbl_Srv::tcpOpen(int a_iPort)		//we can pass the a_iPort number here.
{
   unsigned int iYes=1;
   int iFvalue;
   struct sockaddr_in SrvAddr;
   unsigned int iSinSize;

   // for SNBUF setting
   unsigned int iBuflen = 8192;		// set to 8K //10*65536;  //100 * 1024;
   unsigned int iGetBuflen ; 		//= 131072; //65536;  //100 * 1024;
   int iSizeLen=sizeof(iGetBuflen);

   SrvAddr.sin_family	=	AF_INET;
   SrvAddr.sin_port	=	htons(a_iPort);	//set the a_iPort as the default a_iPort
   SrvAddr.sin_addr.s_addr=	INADDR_ANY;		//let server automatically fill with server IP
								//so that, we need not fill the exact address of server when opening
   memset(&(SrvAddr.sin_zero),'\0',8);

   if((m_iTcpsocket0=socket(AF_INET, SOCK_STREAM, 0))==-1)
   {
      perror("socket");
      exit(-1);
   }

   cout<<"open socket correct: tcpsocket0_="<<m_iTcpsocket0<<endl;

   //set the socket option to reuseaddr: 0 success, -1 failure
   if ( setsockopt(m_iTcpsocket0, SOL_SOCKET, SO_REUSEADDR, &iYes, sizeof(unsigned int)) <0 )
   {
      printf(" Error setting SOL_SOCKET option to 0\n");
      fflush(stdout);
   }

   // bind: 0 success, -1 failure
   if(bind(m_iTcpsocket0,  (struct sockaddr*)&SrvAddr , sizeof(struct sockaddr)) < 0)
   {
      perror("bind");
      exit(-1);
   }
   //listen: 0 success, -1 failure
   if (listen(m_iTcpsocket0, LISTEN_NUM) < 0)
   {
      perror("listen");
      exit(-1);
   }

   // Accept connections coming to the server on the specified a_iPort
   // accept---- -1 failure, otherwise, socket descriptor that server process can
   // use to communicate with the client exclusively
   iSinSize = sizeof(struct sockaddr_in);

   if((m_iTcpsocket=accept(m_iTcpsocket0,(struct sockaddr*)&m_PeerAddr, &iSinSize))==-1)
   {
      perror("accept");
   }

   // test whether SNDBUF works
   if (setsockopt(m_iTcpsocket, SOL_SOCKET, SO_SNDBUF,  &iBuflen, sizeof(iBuflen)) < 0 )
   {
      printf(" Error setting SOL_SOCKET option to 0\n");
      fflush(stdout);
   }

   //need to check whether the set take effect.
   if (getsockopt(m_iTcpsocket, SOL_SOCKET, SO_SNDBUF,  &iGetBuflen, &(socklen_t)iSizeLen) < 0 )
   {
      printf(" Error setting SOL_SOCKET option to 0\n");
      fflush(stdout);
   }
   else
   {
	printf("TCP socket SNDBUF is set to be %d rather than %d \n", iGetBuflen, iBuflen);
   }
}

void Sbl_Srv::udpOpen(int a_iPort)
{
   //change the UDP sender buffer here !!!!!!!!!
   unsigned int iBuflen = SND_BUF_SIZE;		//65536;		1310720; //65536;  //100 * 1024;
   unsigned int iGetBuflen ; 		//= 131072; //65536;  //100 * 1024;
   int iSizeLen=sizeof(iGetBuflen);

   if ((m_iUdpsocket= socket(AF_INET, SOCK_DGRAM, 0)) < 0)
   {
      perror("socket");
      exit(-1);
   }

   /*the a_iPort number filled is tcp number, we need to change it to UDP a_iPort */
   m_PeerAddr.sin_port=htons(a_iPort); //htons(m_uConnectInfo.m_lUdpport);

   if (connect(m_iUdpsocket,(struct sockaddr*)&m_PeerAddr, sizeof(struct sockaddr)) < 0 )
   {
      perror(" connect error ");
      exit(-1);
   }
   cout<<"After connect: m_iUdpsocket= "<<m_iUdpsocket<<endl;
   cout<<"Server connect to m_PeerAddr a_iPort: "<<ntohs(m_PeerAddr.sin_port)<<endl;

   // Client only used for send data,
   // so set SO_SNDBUF as 1MB.
   // setscokopt()--  0: success, -1: failure
   if (setsockopt(m_iUdpsocket, SOL_SOCKET, SO_SNDBUF,  &iBuflen, sizeof(iBuflen)) < 0 )
   {
      printf(" Error setting SOL_SOCKET option to 0\n");
      fflush(stdout);
   }

   //need to check whether the set take effect.
   if (getsockopt(m_iUdpsocket, SOL_SOCKET, SO_SNDBUF,  &iGetBuflen, &(socklen_t)iSizeLen) < 0 )
   {
      printf(" Error setting SOL_SOCKET option to 0\n");
      fflush(stdout);
   }
   else
   {
     printf("UDP socket SNDBUF is set to be %d rather than %d \n", iGetBuflen, iBuflen);
   }
}

//Not really sending data, just get the data ready for udpThread
void Sbl_Srv::Send_Data(void* a_pcPtr, int a_iSize)
{
   BufLink * upTemp=new BufLink;
   upTemp->m_pcPtr=(char*)a_pcPtr;
   upTemp->m_iSize=a_iSize;
   upTemp->m_uNext=NULL;

   if(m_uHead==NULL)
   {
      m_uHead=upTemp;
      m_uTail=upTemp;
   }
   else
   {
      m_uTail->m_uNext=upTemp;
      m_uTail=upTemp;
   }

   if (m_uCurr == NULL)
      m_uCurr = m_uTail;

         pthread_mutex_lock(&_srvBufNolock);
   	 m_iBufNo++;
         pthread_mutex_unlock(&_srvBufNolock);

   while(m_iBufNo>MAX_BLK_NUM)usleep(1);	//if too much data, wait here for a while
						//so that the upper layer can not send more data
   return;
}






/**************************************************************************
 *
 *	Member functions of Sbl_Clt		(Part 33333333333333)
 *
 *****************************************************************************************/
Sbl_Clt::Sbl_Clt()
{
   //Sbl::Sbl();
}



Sbl_Clt::~Sbl_Clt()
{
   delete [] m_apiRecvpktlist[0];
   delete [] m_apiRecvpktlist[1];
   delete m_Lastrecvtime;
   delete m_Lastsyntime;
   delete m_Currtime;
}

void Sbl_Clt::feedBack(int a_lType)
{
   long lSynInt;
   int iEnd;
   int i, j;
   TcpData *puScData = new TcpData;

   puScData->m_lType = a_lType;

   //cout << "feedBack: " << m_lType << "   " << m_alPktnofrom[m_iActivebuf] <<endl;
   switch (a_lType)
   {
   case SC_END:
      puScData->m_lLength = 0;
      break;
   case SC_SYN:
      gettimeofday(m_Currtime, 0);
      lSynInt = 1000000 * (m_Currtime->tv_sec - m_Lastsyntime->tv_sec) + (m_Currtime->tv_usec - m_Lastsyntime->tv_usec);
      gettimeofday(m_Lastsyntime, 0);
      puScData->m_lRecvRate = (long)((double)m_lLocalrecv / ((double)lSynInt / 1000000.0) * 10000);
      pthread_mutex_lock(&_cltlock);
      m_lLocalrecv = 0;
      pthread_mutex_unlock(&_cltlock);
      if (!m_abNoeOccured[m_iActivebuf])
      {
         puScData->m_lLength = 0;
         break;
      }

   case SC_EXP:
   case SC_NOE:
      iEnd = (SC_EXP == a_lType) ? m_uConnectInfo.m_lSockBuffsize: m_aiExpect[m_iActivebuf] + 1;

      j = 0;

      for (i = 0; i < iEnd; i ++)
         if (0 == m_apiRecvpktlist[m_iActivebuf][i])
         {
            puScData->m_alLoss[j++] = i;
               //-------- if (j == MAX_LOSS_SEQ)
               if (j ==( m_uConnectInfo.m_lPacksize/sizeof(unsigned long) -3) )
                  break;
         }

      puScData->m_lLength = j;
      if((puScData->m_lLength==0)&&(a_lType==SC_NOE))
      {
		delete puScData;
		return;
      }
      cout << "Loss Length: " << puScData->m_lLength << endl;
      m_abNoeOccured[m_iActivebuf] = false;

      break;
   case SC_ERR:

      break;
   }

   //-------myHtonl((unsigned long *)puScData, MAX_LOSS_SEQ + 3);
   //-------tcpWrite(puScData, MAX_PKT_SIZE);
   myHtonl((unsigned long *)puScData, puScData->m_lLength  + 3);
   tcpWrite(puScData, m_uConnectInfo.m_lPacksize );


   delete puScData;
}

void Sbl_Clt::initCltBuf(int a_iBufno)
{
   m_apcCltbuf[a_iBufno]=m_uCurr->m_pcPtr;
   m_uCurr=m_uCurr->m_uNext;

   m_aiLostlen[a_iBufno] = m_uConnectInfo.m_lSockBuffsize;
   m_aiExpect[a_iBufno] = 0;
   m_abNoeOccured[a_iBufno] = false;
   if (m_iBufinwork == 0)
      m_aiPktnofrom[a_iBufno] = m_lAckedBlkSeq;
   else if(m_iBufinwork ==1)
      m_aiPktnofrom[a_iBufno] = m_lAckedBlkSeq + m_uConnectInfo.m_lSockBuffsize;
   //memset(m_aplRecvpktlist[a_iBufno], 0, m_uConnectInfo.m_lSockBuffsize* sizeof(int));
   for (int i = 0; i < m_uConnectInfo.m_lSockBuffsize; i ++)
      m_apiRecvpktlist[a_iBufno][i] = 0;

   m_iBufinwork++;
}

//This thread exchange information between the server and client using TCP
void Sbl_Clt::tcpThread()
{
   cout<<"Client in TCP Thread"<<endl;

   timeval *tv1 = new timeval;
   timeval *tv2 = new timeval;
   timespec *req = new timespec;
   req->tv_sec = 0;
   req->tv_nsec = m_lNoeInterval;
   long lCounter = 0;

   gettimeofday(tv1, 0);
   gettimeofday(m_Lastsyntime, 0);

   while (true)
   {
      gettimeofday(tv1, 0);

      if (0 == ((++ lCounter) % 50))
         feedBack(SC_SYN);
      else
      {
         if ((1000000 * (tv1->tv_sec - m_Lastrecvtime->tv_sec) + (tv1->tv_usec - m_Lastrecvtime->tv_usec)) > m_lExpInterval)
         {
            feedBack(SC_EXP);
            gettimeofday(m_Lastrecvtime, 0);
         }

         if (m_abNoeOccured[m_iActivebuf])
            feedBack(SC_NOE);
      }

      gettimeofday(tv2, 0);
      while (m_lNoeInterval > 1000000 * (tv2->tv_sec - tv1->tv_sec) + (tv2->tv_usec - tv1->tv_usec))
      {
         usleep(1);
         gettimeofday(tv2, 0);
      }
      //nanosleep(req, NULL);
   }

   delete req;

   cout<<"TCP Thread in Client closing"<<endl;
}


//*******************************************************************
//UDP receiving data packet
void Sbl_Clt::udpThread()
{
   unsigned long lSeqno;
   int iRecvbufno = 0;

   struct packet
   {
      long m_lSeqno;
      long m_alData[PAYLOAD_SIZE/4];
   } uUdpdata;

   while (true)
   {
      while(!m_aiLostlen[m_iActivebuf])
      {
         m_aiLostlen[m_iActivebuf] = m_uConnectInfo.m_lSockBuffsize;
         m_iBufrecved++;
         m_lAckedBlkSeq += m_uConnectInfo.m_lSockBuffsize;
         m_iBufinwork --;
         m_iActivebuf = 1 - m_iActivebuf;
         pthread_mutex_lock(&_cltBufNolock);
         m_iBufNo --;
         pthread_mutex_unlock(&_cltBufNolock);
         feedBack(SC_END);
      }
      if (m_iBufinwork== 0)
      {
         while (m_iBufNo == 0) {}
         initCltBuf(m_iActivebuf);
      }
      if ((m_iBufinwork== 1) && (m_iBufNo > 1))
      {
         initCltBuf(1 - m_iActivebuf);
      }

      //------- read(m_iUdpsocket, &uUdpdata, MAX_PKT_SIZE);
      read(m_iUdpsocket, &uUdpdata, m_uConnectInfo.m_lPacksize );
      lSeqno = uUdpdata.m_lSeqno;		//??????????????

      gettimeofday(m_Lastrecvtime, 0);
      //cout << "recv a_lSeqno: " << a_lSeqno <<endl;

      pthread_mutex_lock(&_cltlock);
      m_lLocalrecv ++;
      pthread_mutex_unlock(&_cltlock);

      int i;
      if((m_iBufinwork==1)&&((lSeqno<m_aiPktnofrom[m_iActivebuf])||(lSeqno>=(m_aiPktnofrom[m_iActivebuf]+m_uConnectInfo.m_lSockBuffsize))))
         continue;

      if ((lSeqno >= m_aiPktnofrom[0]) && (lSeqno < m_aiPktnofrom[0] + m_uConnectInfo.m_lSockBuffsize))
         iRecvbufno = 0;
      else if ((lSeqno >= m_aiPktnofrom[1]) && (lSeqno < m_aiPktnofrom[1] + m_uConnectInfo.m_lSockBuffsize))
         iRecvbufno = 1;
      else
         continue;

      lSeqno -= m_aiPktnofrom[iRecvbufno];

      if (0 == m_apiRecvpktlist[iRecvbufno][lSeqno])
      {
         m_apiRecvpktlist[iRecvbufno][lSeqno] = 1;

         //---------- memcpy(m_apcCltbuf[iRecvbufno] + lSeqno * PAYLOAD_SIZE, uUdpdata.m_alData, PAYLOAD_SIZE);
         memcpy(m_apcCltbuf[iRecvbufno] + lSeqno * ( m_uConnectInfo.m_lPacksize - sizeof(unsigned long)) , uUdpdata.m_alData, ( m_uConnectInfo.m_lPacksize - sizeof(unsigned long)) );

         if (lSeqno > m_aiExpect[iRecvbufno])
            m_abNoeOccured[iRecvbufno] = true;

         m_aiLostlen[iRecvbufno] --;

         if (m_aiLostlen[iRecvbufno])
            calcNextExpt(lSeqno, iRecvbufno);
      }
   }
}

//*******************************************************************/




/********************************************************************
void Sbl_Clt::udpThread()
{
   unsigned long lSeqno;
   int iRecvbufno = 0;

   struct packet
   {
      long m_lSeqno;
      long m_alData[PAYLOAD_SIZE/4];
   } uUdpdata;

   while (true)
   {
      while(!m_aiLostlen[m_iActivebuf])
      {
         m_aiLostlen[m_iActivebuf] = m_uConnectInfo.m_lSockBuffsize;
         m_iBufrecved++;
         m_lAckedBlkSeq += m_uConnectInfo.m_lSockBuffsize;
         m_iBufinwork --;
         m_iActivebuf = 1 - m_iActivebuf;
         pthread_mutex_lock(&_cltBufNolock);
         m_iBufNo --;
         pthread_mutex_unlock(&_cltBufNolock);
         feedBack(SC_END);
      }

      if (m_iBufinwork== 0)
      {
         while (m_iBufNo == 0) {}
         initCltBuf(m_iActivebuf);
      }
      if ((m_iBufinwork== 1) && (m_iBufNo > 1))
      {
         initCltBuf(1 - m_iActivebuf);
      }

      m_aPktData[0].iov_base=(void *) &uUdpdata.m_lSeqno;
      m_aPktData[0].iov_len =sizeof(unsigned long);
      m_aPktData[1].iov_base=uUdpdata.m_alData;
      m_aPktData[1].iov_len = m_uConnectInfo.m_lPacksize - sizeof(unsigned long) ;

      recv(m_iUdpsocket, &uUdpdata, 4 ,MSG_PEEK );
      lSeqno = uUdpdata.m_lSeqno;

      gettimeofday(m_Lastrecvtime, 0);

      pthread_mutex_lock(&_cltlock);
      m_lLocalrecv ++;
      pthread_mutex_unlock(&_cltlock);

      int i;
      if((m_iBufinwork==1)&&((lSeqno<m_aiPktnofrom[m_iActivebuf])||(lSeqno>=(m_aiPktnofrom[m_iActivebuf]+m_uConnectInfo.m_lSockBuffsize))))
	{
   		readv(m_iUdpsocket, (const struct iovec *) m_aPktData, 2);
         	continue;
	}

      if ((lSeqno >= m_aiPktnofrom[0]) && (lSeqno < m_aiPktnofrom[0] + m_uConnectInfo.m_lSockBuffsize))
         iRecvbufno = 0;
      else if ((lSeqno >= m_aiPktnofrom[1]) && (lSeqno < m_aiPktnofrom[1] + m_uConnectInfo.m_lSockBuffsize))
         iRecvbufno = 1;
      else
	{
   		readv(m_iUdpsocket, (const struct iovec *) m_aPktData, 2);
         	continue;
	}

      lSeqno -= m_aiPktnofrom[iRecvbufno];

      if (0 == m_apiRecvpktlist[iRecvbufno][lSeqno])
      {
        m_apiRecvpktlist[iRecvbufno][lSeqno] = 1;
   	m_aPktData[1].iov_base=m_apcCltbuf[iRecvbufno] + lSeqno * ( m_uConnectInfo.m_lPacksize - sizeof(unsigned long));
        if (lSeqno > m_aiExpect[iRecvbufno])
           m_abNoeOccured[iRecvbufno] = true;
        m_aiLostlen[iRecvbufno] --;
        if (m_aiLostlen[iRecvbufno])
            calcNextExpt(lSeqno, iRecvbufno);
      }

   	if ( readv(m_iUdpsocket, (const struct iovec *) m_aPktData, 2) < 0 )
   	{
      		perror(" readv");
      		exit(0);
   	}
   }
}

********************************************************************/



void Sbl_Clt::calcNextExpt(int a_iCurrseqno, int a_iBufno)
{
  m_aiExpect[a_iBufno] = a_iCurrseqno;
  while (1 == m_apiRecvpktlist[a_iBufno][m_aiExpect[a_iBufno]])
  {
     m_aiExpect[a_iBufno] ++;
     if (m_aiExpect[a_iBufno] == m_uConnectInfo.m_lSockBuffsize)
        m_aiExpect[a_iBufno] = 0;
  }
}

/***************************************************************************
 *
 *   Estabilish the connection, in the server side it need to:
 *	1)   Let server listen to connect attempt.
 *	2)
 *
 **************************************************************************/

void Sbl_Clt::Connect_Open(char* a_pcSrvAddrStr, int a_iPort)
{
   ConnectControl uTempinfo;

   tcpOpen(a_pcSrvAddrStr,a_iPort);	//open TCP connect

   //The following exchange information between Client and Server
   uTempinfo=m_uConnectInfo;

   myHtonl((unsigned long*) &uTempinfo, sizeof(ConnectControl)/sizeof(unsigned long));

   tcpWrite((void*) &uTempinfo, sizeof(ConnectControl));
   tcpRead ((void*) &uTempinfo, sizeof(ConnectControl));
   myNtohl((unsigned long*) &uTempinfo, sizeof(ConnectControl)/sizeof(unsigned long));

   setConnInfo(uTempinfo);


//----------- moved from constructor
   m_apcCltbuf[0] = NULL;
   m_apcCltbuf[1] = NULL;
   m_apiRecvpktlist[0] = new int[m_uConnectInfo.m_lSockBuffsize];
   m_apiRecvpktlist[1] = new int[m_uConnectInfo.m_lSockBuffsize];
   m_aiPktnofrom[0] = -m_uConnectInfo.m_lSockBuffsize;
   m_aiPktnofrom[1] = -m_uConnectInfo.m_lSockBuffsize;
   m_iActivebuf = 0;
   m_abNoeOccured[0] = false;
   m_abNoeOccured[1] = false;
   m_aiLostlen[0] = 1000;
   m_aiLostlen[1] = 1000;

   //rate control parameters
   m_lExpInterval = 100000;
   m_lNoeInterval = 10000;			//about 50*10000=0.5s for each SYN

   m_Lastrecvtime = new timeval;
   m_Lastsyntime = new timeval;
   m_Currtime = new timeval;
   gettimeofday(m_Lastrecvtime, 0);
   m_Lastrecvtime->tv_sec += 1000;
   m_iBufrecved=0;

   //--------- m_puScData = new TcpData;

//--------

   //udpOpen(a_iPort + 1);
   udpOpen(m_uConnectInfo.m_lUdpport);
   cout<<"UDP connection on port: "<< m_uConnectInfo.m_lUdpport<<endl;

   //Create the Thread to sent lost packets info and SYN info back to Server
   m_pTcpthreadp=(pthread_t*)malloc(sizeof(pthread_t));
   pthread_create(m_pTcpthreadp,NULL,::tcpCltCaller,(void *)this);

   //Create the Thread to receive data packet
   m_pUdpthreadp=(pthread_t*)malloc(sizeof(pthread_t));
   pthread_create(m_pUdpthreadp,NULL,::udpCltCaller,(void *)this);
}

void Sbl_Clt::tcpOpen(char* a_pcSrvAddrStr, int a_iPort=0)
{
   struct hostent *srv;
   unsigned int	iYes=1;
   int iFvalue;

   //For SNDBUF setting
   unsigned int iBuflen = 65536/4;  //100 * 1024;
   unsigned int iGetBuflen ; 		//= 131072; //65536;  //100 * 1024;
   int iSizeLen=sizeof(iGetBuflen);

   if((srv=gethostbyname(a_pcSrvAddrStr))==NULL)
   {
      perror("gethostbyname");
      exit(-1);
   }

   if((m_iTcpsocket=socket(AF_INET, SOCK_STREAM, 0))==-1)
   {
      perror("unable to open socket");
      exit(-1);
   }

   cout<<"Open socket correct: m_iTcpsocket="<<m_iTcpsocket<<endl;

   //get socket control
   if( (iFvalue = fcntl( m_iTcpsocket, F_GETFL, 0 ) ) < 0  )
   {
      printf( "Error in getting options\n" );
      fflush( stdout );
   }

   //set sockd BLOCKING
   iFvalue &= ~O_NONBLOCK;
   if( fcntl( m_iTcpsocket, F_SETFL, iFvalue) < 0 )
   {
      printf( "Error in setting options\n");
      fflush( stdout );
   }

   m_PeerAddr.sin_family	=AF_INET;
   if(a_iPort<=0)	a_iPort=DEFAULT_PORT;
   m_PeerAddr.sin_port	=htons(a_iPort);
   m_PeerAddr.sin_addr	=*((struct in_addr *)srv->h_addr);
   memset(&(m_PeerAddr.sin_zero),'\0', 8);

   // connect()--- -1 failure, 0 success
   if(connect(m_iTcpsocket, (struct sockaddr*)&m_PeerAddr, sizeof(struct sockaddr))==-1)
   {
      perror("connect");
   }

   if ( setsockopt(m_iTcpsocket, SOL_TCP, TCP_NODELAY, &iYes, sizeof(unsigned int) ) < 0 )
   {
      printf( "Error setting TCP_PPNODELAY option to 0\n" );
      fflush( stdout );
   }

   // test whether SNDBUF works
   if (setsockopt(m_iTcpsocket, SOL_SOCKET, SO_SNDBUF,  &iBuflen, sizeof(iBuflen)) < 0 )
   {
      printf(" Error setting SOL_SOCKET option to 0\n");
      fflush(stdout);
   }

   //need to check whether the set take effect.
   if (getsockopt(m_iTcpsocket, SOL_SOCKET, SO_SNDBUF,  &iGetBuflen, &(socklen_t)iSizeLen) < 0 )
   {
      printf(" Error setting SOL_SOCKET option to 0\n");
      fflush(stdout);
   }
   else
   {
      printf("TCP socket SNDBUF is set to be %d rather than %d \n", iGetBuflen, iBuflen);
   }
}

void Sbl_Clt::udpOpen(int a_iPort)
{
   struct sockaddr_in myAddr;
   unsigned int iBuflen = RCV_BUF_SIZE;
   unsigned int iGetBuflen;
   int iSizeLen=sizeof(iGetBuflen);

   myAddr.sin_family = AF_INET;
   myAddr.sin_port = htons(a_iPort);

   cout<<"Port="<<m_uConnectInfo.m_lUdpport<<endl;

   myAddr.sin_addr.s_addr = INADDR_ANY;

   memset(&(myAddr.sin_zero),'\0',8);

   if ((m_iUdpsocket= socket(AF_INET, SOCK_DGRAM, 0)) < 0)
   {
      perror("socket");
      exit(-1);
   }

   cout<<"before UDP bind: m_iUdpsocket="<<m_iUdpsocket<<endl;

   if( bind(m_iUdpsocket, (struct sockaddr*) &myAddr, sizeof(struct sockaddr)) < 0 )
   {
      perror(" bind error ");
      exit(-1);
   }
   cout<<"Client bind to selfport :"<<ntohs(myAddr.sin_port)<<endl;

   // In our program, server only used for receive data,
   // so set SO_RCVBUF as 1MB.
   // setscokopt()--  0: success, -1: failure

   //iBuflen = 4*1024*1024;		//1000*1500; //92160
   if ( setsockopt(m_iUdpsocket, SOL_SOCKET, SO_RCVBUF,  &iBuflen, sizeof(iBuflen)) < 0 )
   {
      printf(" Error setting SOL_SOCKET option to 0 \n");
      fflush(stdout);
   }

   //Need to check whether the setsockopt in effect correctly
   if (getsockopt(m_iUdpsocket, SOL_SOCKET, SO_RCVBUF,  &iGetBuflen, &(socklen_t)iSizeLen) < 0 )
   {
      printf(" Error setting SOL_SOCKET option to 0\n");
      fflush(stdout);
   }
   else
   {
      printf("UDP socket RCVBUF is set to be %d rather than %d \n", iGetBuflen, iBuflen);
   }
}

void Sbl_Clt::Prov_Buff(void *a_pcPtr, int a_iSize)
{
   BufLink* upTemp=new BufLink;
   upTemp->m_pcPtr=(char*)a_pcPtr;
   upTemp->m_iSize=a_iSize;
   upTemp->m_uNext=NULL;

   if(m_uHead==NULL)
   {
      m_uHead=upTemp;
      m_uTail=upTemp;
      m_uCurr=upTemp;
   }
   else
   {
      m_uTail->m_uNext=upTemp;
      m_uTail=upTemp;
   }
   if(m_uCurr==NULL)
      m_uCurr=m_uTail;

   pthread_mutex_lock(&_cltBufNolock);
   m_iBufNo++;
   pthread_mutex_unlock(&_cltBufNolock);
}

/*******************************************************************
 *	This function receives the 'a_iSize' amount of data
 *	It does not really receive,
 *	It just check whether the underline Udp_thread has received apropriate amount data
 *	If the amount has been received, it let the APPLICATION to handle it.
 *	The function itself will do nothing about the data.
 ********************************************************************/
void * Sbl_Clt::Recv_Data(int& a_iSize)		//assume the a_iSize is exactly the bytes in a block
{
   timespec *ts = new timespec;
   ts->tv_sec = 0;
   ts->tv_nsec = 1;
   void *pTempPtr;
   BufLink* upTemp;
   //unsigned long bytes_in_block=m_uConnectInfo.m_lSockBuffsize*m_uConnectInfo.m_lPacksize;

   while(m_iBufrecved<=0)
   {
      nanosleep(ts, NULL);
   }		//may not be good.

   while(m_iBufrecved <= 0){}
   m_iBufrecved--;
   if(a_iSize!=m_uHead->m_iSize)cout<<"Recv a_iSize error"<<endl;
   pTempPtr=m_uHead->m_pcPtr;
   upTemp=m_uHead;
   m_uHead=m_uHead->m_uNext;
   delete [] upTemp;

   delete ts;
   return pTempPtr;
}



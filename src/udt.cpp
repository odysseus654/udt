/*****************************************************************************
Copyright � 2001 - 2005, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) version 2

Laboratory for Advanced Computing (LAC)
National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.lac.uic.edu/

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or (at
your option) any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
*****************************************************************************/

/*****************************************************************************
This file contains the implementation of main algorithms of UDT protocol and
the implementation of core UDT interfaces.

Reference:
UDT programming manual
UDT protocol specification (draft-gg-udt-xx.txt)
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [ygu@cs.uic.edu], last updated 01/14/2005

modified by
   <programmer's name, programmer's email, last updated mm/dd/yyyy>
   <descrition of changes>
*****************************************************************************/


#ifndef WIN32
   #include <unistd.h>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <cerrno>
   #include <cstring>
   #include <cstdlib>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif

#include <cmath>
#include "udt.h"

using namespace std;

CUDTUnited CUDT::s_UDTUnited; 
const UDTSOCKET UDT::INVALID_UDTSOCK = CUDT::INVALID_UDTSOCK;
const int UDT::UDT_ERROR = CUDT::UDT_ERROR;

CUDT::CUDT():
//
// These constants are defined in UDT specification. They MUST NOT be changed!
//
m_iVersion(2),
m_iSYNInterval(10000),
m_iMaxSeqNo(1 << 30),
m_iSeqNoTH(1 << 29),
m_iMaxAckSeqNo(1 << 16),
m_iProbeInterval(16),
m_iQuickStartPkts(16)
{
   m_pChannel = NULL;
   m_pSndBuffer = NULL;
   m_pRcvBuffer = NULL;
   m_pSndLossList = NULL;
   m_pRcvLossList = NULL;
   m_pTimer = NULL;
   m_pIrrPktList = NULL;
   m_pACKWindow = NULL;
   m_pSndTimeWindow = NULL;
   m_pRcvTimeWindow = NULL;

   // Initilize mutex and condition variables
   initSynch();

   // Default UDT configurations
   m_iMSS = 1500;
   m_bSynSending = true;
   m_bSynRecving = true;
   m_pCC = NULL;
   m_iFlightFlagSize = 25600;
   m_iSndQueueLimit = 40960000;
   m_iUDTBufSize = 40960000;
   m_Linger.l_onoff = 1;
   m_Linger.l_linger = 1;
   m_iUDPSndBufSize = 65536;
   m_iUDPRcvBufSize = 4 * 1024 * 1024;
   m_iIPversion = 4;

   m_iRTT = 10 * m_iSYNInterval;
   m_iRTTVar = m_iRTT >> 1;
   m_ullCPUFrequency = CTimer::getCPUFrequency();

   // Initial status
   m_bOpened = false;
   m_bConnected = false;
   m_bBroken = false;
}

CUDT::CUDT(const CUDT& ancestor):
m_iVersion(ancestor.m_iVersion),
m_iSYNInterval(ancestor.m_iSYNInterval),
m_iMaxSeqNo(ancestor.m_iMaxSeqNo),
m_iSeqNoTH(ancestor.m_iSeqNoTH),
m_iMaxAckSeqNo(ancestor.m_iMaxAckSeqNo),
m_iProbeInterval(ancestor.m_iProbeInterval),
m_iQuickStartPkts(ancestor.m_iQuickStartPkts)
{
   m_pChannel = NULL;
   m_pSndBuffer = NULL;
   m_pRcvBuffer = NULL;
   m_pSndLossList = NULL;
   m_pRcvLossList = NULL;
   m_pTimer = NULL;
   m_pIrrPktList = NULL;
   m_pACKWindow = NULL;
   m_pSndTimeWindow = NULL;
   m_pRcvTimeWindow = NULL;

   // Initilize mutex and condition variables
   initSynch();

   // Default UDT configurations
   m_iMSS = ancestor.m_iMSS;
   m_bSynSending = ancestor.m_bSynSending;
   m_bSynRecving = ancestor.m_bSynRecving;
   m_pCC = ancestor.m_pCC;
   m_iFlightFlagSize = ancestor.m_iFlightFlagSize;
   m_iSndQueueLimit = ancestor.m_iSndQueueLimit;
   m_iUDTBufSize = ancestor.m_iUDTBufSize;
   m_Linger = ancestor.m_Linger;
   m_iUDPSndBufSize = ancestor.m_iUDPSndBufSize;
   m_iUDPRcvBufSize = ancestor.m_iUDPRcvBufSize;
   m_iIPversion = ancestor.m_iIPversion;

   m_iRTT = ancestor.m_iRTT;
   m_iRTTVar = ancestor.m_iRTTVar;
   m_ullCPUFrequency = ancestor.m_ullCPUFrequency;

   // Initial status
   m_bOpened = false;
   m_bConnected = false;
   m_bBroken = false;
}

CUDT::~CUDT()
{
   // release mutex/condtion variables
   destroySynch();

   // destroy the data structures
   if (m_pChannel)
      delete m_pChannel;
   if (m_pSndBuffer)
      delete m_pSndBuffer;
   if (m_pRcvBuffer)
      delete m_pRcvBuffer;
   if (m_pSndLossList)
      delete m_pSndLossList;
   if (m_pRcvLossList)
      delete m_pRcvLossList;
   if (m_pTimer)
      delete m_pTimer;
   if (m_pIrrPktList)
      delete m_pIrrPktList;
   if (m_pACKWindow)
      delete m_pACKWindow;
   if (m_pSndTimeWindow)
      delete m_pSndTimeWindow;
   if (m_pRcvTimeWindow)
      delete m_pRcvTimeWindow;
}

void CUDT::setOpt(UDTOpt optName, const void* optval, const __int32& optlen)
{
   CGuard cg(m_ConnectionLock);
   CGuard sendguard(m_SendLock);
   CGuard recvguard(m_RecvLock);

   switch (optName)
   {
   case UDT_MSS:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iMSS = *(__int32 *)optval;
      if (m_iMSS < 28)
         throw CUDTException(5, 3, 0);
      break;

   case UDT_SNDSYN:
      m_bSynSending = *(bool *)optval;
      break;

   case UDT_RCVSYN:
      m_bSynRecving = *(bool *)optval;
      break;

   case UDT_CC:
      #ifndef CUSTOM_CC
         throw CUDTException(5, 1, 0);
      #else
         if (m_bOpened)
            throw CUDTException(5, 1, 0);
         m_pCC = (CCC *)optval;
         m_pCC->m_UDT = m_SocketID;
         m_pCC->m_pUDT = this;
      #endif

      break;

   case UDT_FC:
      if (m_bConnected)
         throw CUDTException(5, 2, 0);

      m_iFlightFlagSize = *(__int32 *)optval;
      if (m_iFlightFlagSize < 1)
         throw CUDTException(5, 3);
      break;

   case UDT_SNDBUF:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iSndQueueLimit = *(__int32 *)optval;
      if (m_iSndQueueLimit <= 0)
         throw CUDTException(5, 3, 0);
      break;

   case UDT_RCVBUF:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iUDTBufSize = *(__int32 *)optval;
      if (m_iUDTBufSize < (m_iMSS - 28) * 16)
         throw CUDTException(5, 3, 0);
      break;

   case UDT_LINGER:
      m_Linger = *(linger*)optval;
      break;

   case UDP_SNDBUF:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iUDPSndBufSize = *(__int32 *)optval;
      break;

   case UDP_RCVBUF:
      if (m_bOpened)
         throw CUDTException(5, 1, 0);

      m_iUDPRcvBufSize = *(__int32 *)optval;
      break;

   default:
      throw CUDTException(5, 0, 0);
   }
}

void CUDT::getOpt(UDTOpt optName, void* optval, __int32& optlen)
{
   CGuard cg(m_ConnectionLock);

   switch (optName)
   {
   case UDT_MSS:
      *(__int32 *)optval = m_iMSS;
      optlen = sizeof(__int32);
      break;

   case UDT_SNDSYN:
      *(bool *)optval = m_bSynSending;
      optlen = sizeof(bool);
      break;

   case UDT_RCVSYN:
      *(bool *)optval = m_bSynRecving;
      optlen = sizeof(bool);
      break;

   case UDT_CC:
      #ifndef CUSTOM_CC
         throw CUDTException(5, 1, 0);
      #else
         (CCC *)optval = m_pCC;
         optlen = sizeof(CCC);
      #endif

      break;

   case UDT_FC:
      *(__int32 *)optval = m_iFlightFlagSize;
      optlen = sizeof(__int32);
      break;

   case UDT_SNDBUF:
      *(__int32 *)optval = m_iUDTBufSize;
      optlen = sizeof(__int32);
      break;

   case UDT_RCVBUF:
      *(__int32 *)optval = m_iUDTBufSize;
      optlen = sizeof(__int32);
      break;

   case UDT_LINGER:
      *(linger*)optval = m_Linger;
      optlen = sizeof(linger);
      break;

   case UDP_SNDBUF:
      *(__int32 *)optval = m_iUDPSndBufSize;
      optlen = sizeof(__int32);
      break;

   case UDP_RCVBUF:
      *(__int32 *)optval = m_iUDPRcvBufSize;
      optlen = sizeof(__int32);
      break;

   default:
      throw CUDTException(5, 0, 0);
   }
}

void CUDT::open(const sockaddr* addr)
{
   CGuard cg(m_ConnectionLock);

   if (m_bOpened)
      close();

   // Initial status
   m_bClosing = false;
   m_bShutdown = false;
   m_bListening = false;
   m_iEXPCount = 1;

   // Initial sequence number, loss, acknowledgement, etc.
   m_iPktSize = m_iMSS - 28;
   m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;
   m_iISN = 0;
   m_iPeerISN = 0;
 
   m_bLoss = false;
   gettimeofday(&m_LastSYNTime, 0);

   m_iSndLastAck = 0;
   m_iSndLastDataAck = 0;
   m_iSndCurrSeqNo = -1;

   m_iRcvLastAck = 0;
   m_iRcvLastAckAck = 0;
   m_ullLastAckTime = 0;
   m_iRcvCurrSeqNo = -1;
   m_iNextExpect = 0;
   m_bReadBuf = false;

   m_iLastDecSeq = -1;
   m_iNAKCount = 0;
   m_iDecRandom = 1;
   m_iAvgNAKNum = 1;

   m_iBandwidth = 1;
   m_bSndSlowStart = true;
   m_bRcvSlowStart = true;
   m_bFreeze = false;

   m_iAckSeqNo = 0;

   m_iSndHandle = (1 << 30);
   m_iRcvHandle = -(1 << 30);

   // Initial sending rate = 1us
   m_ullInterval = m_ullCPUFrequency;
   m_ullTimeDiff = 0;
   m_ullLastDecRate = m_ullCPUFrequency;

   // default congestion window size = infinite
   m_dCongestionWindow = 1 << 30;

   // Initial Window Size = 16 packets
   m_iFlowWindowSize = 16;
   m_iFlowControlWindow = 16;
   m_iMaxFlowWindowSize = m_iFlightFlagSize;

   #ifdef CUSTOM_CC
      if (NULL != m_pCC)
      {
         m_pCC->init();
         m_ullInterval = (unsigned __int64)(m_pCC->m_dPktSndPeriod * m_ullCPUFrequency);
         m_dCongestionWindow = m_pCC->m_dCWndSize;
      }
   #endif

   #ifdef TRACE
      // trace information
      gettimeofday(&m_StartTime, 0);
      m_llSentTotal = m_llRecvTotal = m_iLossTotal = m_iRetransTotal = m_iSentACKTotal = m_iRecvACKTotal = m_iSentNAKTotal = m_iRecvNAKTotal = 0;
      gettimeofday(&m_LastSampleTime, 0);
      m_llTraceSent = m_llTraceRecv = m_iTraceLoss = m_iTraceRetrans = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = 0;
   #endif

   // Construct and open a channel
   try
   {
      m_pChannel = new CChannel(m_iIPversion);

      m_pChannel->setSndBufSize(m_iUDPSndBufSize);
      m_pChannel->setRcvBufSize(m_iUDPRcvBufSize);

      m_pChannel->open(addr);
   }
   catch(CUDTException e)
   {
      // Let applications to process this exception
      throw CUDTException(e);
   }

   // Now UDT is opened.
   m_bOpened = true;
}

#ifndef WIN32
void* CUDT::listenHandler(void* listener)
#else
DWORD WINAPI CUDT::listenHandler(LPVOID listener)
#endif
{
   CUDT* self = static_cast<CUDT*>(listener);

   // Type 0 (handshake) control packet
   CPacket initpkt;
   char* initdata = new char [self->m_iPayloadSize];
   CHandShake* hs = (CHandShake *)initdata;
   initpkt.pack(0, NULL, initdata, sizeof(CHandShake));

   sockaddr* addr;
   if (4 == self->m_iIPversion)
      addr = (sockaddr*)(new sockaddr_in);
   else
      addr = (sockaddr*)(new sockaddr_in6);

   while (!self->m_bClosing)
   {
      // Listening to the port...
      initpkt.setLength(self->m_iPayloadSize);
      if (self->m_pChannel->recvfrom(initpkt, addr) <= 0)
         continue;

      // When a peer side connects in...
      if ((1 == initpkt.getFlag()) && (0 == initpkt.getType()))
         s_UDTUnited.newConnection(self->m_SocketID, addr, hs);
   }

   if (4 == self->m_iIPversion)
      delete (sockaddr_in*)addr;
   else
      delete (sockaddr_in6*)addr;

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

void CUDT::listen()
{
   CGuard cg(m_ConnectionLock);

   if (!m_bOpened)
      throw CUDTException(5, 0, 0);

   if (m_bConnected)
      throw CUDTException(5, 2, 0);

   // listen can be called more than once
   if (m_bListening)
      return;

   #ifndef WIN32
      if (0 != pthread_create(&m_ListenThread, NULL, CUDT::listenHandler, this))
         throw CUDTException(7, 0, errno);
   #else
      if (NULL == (m_ListenThread = CreateThread(NULL, 0, CUDT::listenHandler, this, 0, NULL)))
         throw CUDTException(7, 0, GetLastError());
   #endif

   m_bListening = true;
}

void CUDT::connect(const sockaddr* serv_addr)
{
   CGuard cg(m_ConnectionLock);

   if (!m_bOpened)
      throw CUDTException(5, 0, 0);

   if (m_bListening)
      throw CUDTException(5, 2, 0);

   if (m_bConnected)
      throw CUDTException(5, 2, 0);

   // I will connect to an Initiator, so I am NOT an initiator.
   m_bInitiator = false;

   CPacket initpkt;
   char* initdata = new char [m_iPayloadSize];
   CHandShake* hs = (CHandShake *)initdata;

   // This is my current configurations.
   hs->m_iVersion = m_iVersion;
   hs->m_iMSS = m_iMSS;
   hs->m_iFlightFlagSize = m_iFlightFlagSize;

   // Random Initial Sequence Number
   timeval currtime;
   gettimeofday(&currtime, 0);
   srand(currtime.tv_usec);
   m_iISN = hs->m_iISN = (__int32)(double(rand()) * m_iMaxSeqNo / (RAND_MAX + 1.0));

   m_iLastDecSeq = hs->m_iISN - 1;
   m_iSndLastAck = hs->m_iISN;
   m_iSndLastDataAck = hs->m_iISN;
   m_iSndCurrSeqNo = hs->m_iISN - 1;

   initpkt.pack(0, NULL, initdata, sizeof(CHandShake));
 
   // Inform the initiator my configurations.
   m_pChannel->sendto(initpkt, serv_addr);

   sockaddr* peer_addr;
   if (4 == m_iIPversion)
      peer_addr = (sockaddr*)(new sockaddr_in);
   else
      peer_addr = (sockaddr*)(new sockaddr_in6);

   // Wait for the negotiated configurations from the peer side.
   initpkt.setLength(m_iPayloadSize);
   m_pChannel->recvfrom(initpkt, peer_addr);

   const __int32 timeo = 3000000;

   timeval entertime;
   gettimeofday(&entertime, 0);

   while ((initpkt.getLength() <= 0) || (1 != initpkt.getFlag()) || (0 != initpkt.getType()))
   {
      initpkt.setLength(sizeof(CHandShake));
      m_pChannel->sendto(initpkt, serv_addr);

      initpkt.setLength(m_iPayloadSize);
      m_pChannel->recvfrom(initpkt, peer_addr);

      gettimeofday(&currtime, 0);
      if ((currtime.tv_sec - entertime.tv_sec) * 1000000 + (currtime.tv_usec - entertime.tv_usec) > timeo)
         throw CUDTException(1, 1, 0);
   }

   m_pChannel->connect(peer_addr);

   if (4 == m_iIPversion)
      delete (sockaddr_in*)peer_addr;
   else
      delete (sockaddr_in6*)peer_addr;

   // Got it. Re-configure according to the negotiated values.
   m_iMSS = hs->m_iMSS;
   m_iMaxFlowWindowSize = hs->m_iFlightFlagSize;
   m_iPktSize = m_iMSS - 28;
   m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;

   m_iPeerISN = hs->m_iISN;      

   m_iRcvLastAck = hs->m_iISN;
   m_iRcvLastAckAck = hs->m_iISN;
   m_iRcvCurrSeqNo = hs->m_iISN - 1;
   m_iNextExpect = hs->m_iISN;

   m_iUserBufBorder = m_iRcvLastAck + (__int32)ceil(double(m_iUDTBufSize) / m_iPayloadSize);

   delete [] initdata;

   // Prepare all structures
   m_pTimer = new CTimer;
   m_pSndBuffer = new CSndBuffer;
   m_pRcvBuffer = new CRcvBuffer(m_iUDTBufSize);

   // after introducing lite ACK, the sndlosslist may not be cleared in time, so it requires twice space.
   m_pSndLossList = new CSndLossList(m_iMaxFlowWindowSize * 2, m_iSeqNoTH, m_iMaxSeqNo);

   m_pRcvLossList = new CRcvLossList(m_iFlightFlagSize, m_iSeqNoTH, m_iMaxSeqNo);
   m_pIrrPktList = new CIrregularPktList(m_iFlightFlagSize, m_iSeqNoTH, m_iMaxSeqNo);
   m_pACKWindow = new CACKWindow(4096);
   m_pRcvTimeWindow = new CPktTimeWindow(m_iQuickStartPkts, 16, 64);

   // Now I am also running, a little while after the Initiator was running.
   #ifndef WIN32
      m_bSndThrStart = false;
      if (0 != pthread_create(&m_RcvThread, NULL, CUDT::rcvHandler, this))
         throw CUDTException(1, 3, errno);
   #else
      m_SndThread = NULL;
      if (NULL == (m_RcvThread = CreateThread(NULL, 0, CUDT::rcvHandler, this, 0, NULL)))
         throw CUDTException(1, 3, GetLastError());
   #endif

   // And, I am connected too.
   m_bConnected = true;
}

void CUDT::connect(const sockaddr* peer, const CHandShake* hs)
{
   // This UDT entity is an Initiator, since it is started at the server side.
   m_bInitiator = true;

   // Type 0 (handshake) control packet
   CPacket initpkt;
   CHandShake ci;
   memcpy(&ci, hs, sizeof(CHandShake));
   initpkt.pack(0, NULL, &ci, sizeof(CHandShake));

   // Uses the smaller MSS between the peers        
   if (ci.m_iMSS > m_iMSS)
      ci.m_iMSS = m_iMSS;
   else
      m_iMSS = ci.m_iMSS;

   // exchange info for maximum flow window size
   m_iMaxFlowWindowSize = ci.m_iFlightFlagSize;
   ci.m_iFlightFlagSize = m_iFlightFlagSize;

   m_iPeerISN = ci.m_iISN;

   m_iRcvLastAck = ci.m_iISN;
   m_iRcvLastAckAck = ci.m_iISN;
   m_iRcvCurrSeqNo = ci.m_iISN - 1;
   m_iNextExpect = ci.m_iISN;

   m_pChannel->connect(peer);

   // Random Initial Sequence Number
   timeval currtime;
   gettimeofday(&currtime, 0);
   srand(currtime.tv_usec);
   ci.m_iISN = m_iISN = (__int32)(double(rand()) * m_iMaxSeqNo / (RAND_MAX + 1.0));

   m_iLastDecSeq = m_iISN - 1;
   m_iSndLastAck = m_iISN;
   m_iSndLastDataAck = m_iISN;
   m_iSndCurrSeqNo = m_iISN - 1;

   // Send back the negotiated configurations.
   *m_pChannel << initpkt;
  
   m_iPktSize = m_iMSS - 28;
   m_iPayloadSize = m_iPktSize - CPacket::m_iPktHdrSize;

   m_iUserBufBorder = m_iRcvLastAck + (__int32)ceil(double(m_iUDTBufSize) / m_iPayloadSize);

   // Prepare all structures
   m_pTimer = new CTimer;
   m_pSndBuffer = new CSndBuffer;
   m_pRcvBuffer = new CRcvBuffer(m_iUDTBufSize);
   m_pSndLossList = new CSndLossList(m_iMaxFlowWindowSize * 2, m_iSeqNoTH, m_iMaxSeqNo);
   m_pRcvLossList = new CRcvLossList(m_iFlightFlagSize, m_iSeqNoTH, m_iMaxSeqNo);
   m_pIrrPktList = new CIrregularPktList(m_iFlightFlagSize, m_iSeqNoTH, m_iMaxSeqNo);
   m_pACKWindow = new CACKWindow(4096);
   m_pRcvTimeWindow = new CPktTimeWindow(m_iQuickStartPkts, 16, 64);

   // UDT is now running...
   #ifndef WIN32
      m_bSndThrStart = false;
      if (0 != pthread_create(&m_RcvThread, NULL, CUDT::rcvHandler, this))
         throw CUDTException(1, 3, errno);
   #else
      m_SndThread = NULL;
      if (NULL == (m_RcvThread = CreateThread(NULL, 0, CUDT::rcvHandler, this, 0, NULL)))
         throw CUDTException(1, 3, GetLastError());
   #endif

   // And of course, it is connected.
   m_bConnected = true;
}

void CUDT::close()
{
   CGuard cg(m_ConnectionLock);

   if (!m_bOpened)
      return;

   if (0 != m_Linger.l_onoff)
   {
      timeval t1, t2;
      gettimeofday(&t1, 0);
      t2 = t1;

      while (!m_bBroken && m_bConnected && (m_pSndBuffer->getCurrBufSize() > 0) && ((t2.tv_sec - t1.tv_sec - 1) < m_Linger.l_linger))
      {
         #ifndef WIN32
            usleep(10);
         #else
            Sleep(1);
         #endif

         gettimeofday(&t2, 0);
      }
   }

   // if the connection broken, an exception can be throwed.

   // inform the peer side with a "shutdown" packet
   if ((m_bConnected) && (!m_bShutdown))
      sendCtrl(5);

   // Inform the threads handler to stop.
   m_bClosing = true;
   m_bBroken = true;

   // Signal the sender and recver if they are waiting for data.
   releaseSynch();

   // Wait for the threads to exit.

   #ifndef WIN32
      if (m_bListening)
      {
         pthread_join(m_ListenThread, NULL);
         m_bListening = false;
      }
      if (m_bConnected)
      {
         m_pTimer->interrupt();
         if (m_bSndThrStart)
         {
            pthread_join(m_SndThread, NULL);
            m_bSndThrStart = false;
         }
         pthread_join(m_RcvThread, NULL);
         m_bConnected = false;
      }
   #else
      if (m_bListening)
      {
         WaitForSingleObject(m_ListenThread, INFINITE);
         m_bListening = false;
      }
      if (m_bConnected)
      {
         m_pTimer->interrupt();
         if (NULL != m_SndThread)
         {
            WaitForSingleObject(m_SndThread, INFINITE);
            m_SndThread = NULL;
         }
         WaitForSingleObject(m_RcvThread, INFINITE);
         m_bConnected = false;
      }
   #endif

   // waiting all send and recv calls to stop
   CGuard sendguard(m_SendLock);
   CGuard recvguard(m_RecvLock);

   // Channel is to be destroyed.
   if (m_pChannel)
   {
      m_pChannel->disconnect();
      delete m_pChannel;
      m_pChannel = NULL;
   }

   // And structures released.
   if (m_pSndBuffer)
      delete m_pSndBuffer;
   if (m_pRcvBuffer)
      delete m_pRcvBuffer;
   if (m_pSndLossList)
      delete m_pSndLossList;
   if (m_pRcvLossList)
      delete m_pRcvLossList;
   if (m_pTimer)
      delete m_pTimer;
   if (m_pIrrPktList)
      delete m_pIrrPktList;
   if (m_pACKWindow)
      delete m_pACKWindow;
   if (m_pSndTimeWindow)
      delete m_pSndTimeWindow;
   if (m_pRcvTimeWindow)
      delete m_pRcvTimeWindow;

   m_pSndBuffer = NULL;
   m_pRcvBuffer = NULL;
   m_pSndLossList = NULL;
   m_pRcvLossList = NULL;
   m_pTimer = NULL;
   m_pIrrPktList = NULL;
   m_pACKWindow = NULL;
   m_pSndTimeWindow = NULL;
   m_pRcvTimeWindow = NULL;

   // CLOSED.
   m_bOpened = false;
}

#ifndef WIN32
void* CUDT::sndHandler(void* sender)
#else
DWORD WINAPI CUDT::sndHandler(LPVOID sender)
#endif
{
   CUDT* self = static_cast<CUDT *>(sender);

   CPacket datapkt;
   __int32 payload;
   __int32 offset;

   #ifdef CUSTOM_CC
      __int32 cwnd;
   #endif

   bool probe = false;

   unsigned __int64 entertime;
   #ifdef NO_BUSY_WAITING
      unsigned __int64 currtime;
   #endif
   unsigned __int64 targettime;

   #ifndef WIN32
      timeval now;
      timespec timeout;
   #endif

   while (!self->m_bClosing)
   {
      // Remember the time the last packet is sent.
      self->m_pTimer->rdtsc(entertime);

      // Loss retransmission always has higher priority.
      if ((datapkt.m_iSeqNo = self->m_pSndLossList->getLostSeq()) >= 0)
      {
         // protect m_iSndLastDataAck from updating by ACK processing
         CGuard ackguard(self->m_AckLock);

         if ((datapkt.m_iSeqNo >= self->m_iSndLastDataAck) && (datapkt.m_iSeqNo < self->m_iSndLastDataAck + self->m_iSeqNoTH))
            offset = (datapkt.m_iSeqNo - self->m_iSndLastDataAck) * self->m_iPayloadSize;
         else if (datapkt.m_iSeqNo < self->m_iSndLastDataAck - self->m_iSeqNoTH)
            offset = (datapkt.m_iSeqNo + self->m_iMaxSeqNo - self->m_iSndLastDataAck) * self->m_iPayloadSize;
         else
            continue;

         if ((payload = self->m_pSndBuffer->readData(&(datapkt.m_pcData), offset, self->m_iPayloadSize)) == 0)
            continue;

         #ifdef TRACE
            ++ self->m_iTraceRetrans;
         #endif
      }
      // If no loss, pack a new packet.
      else
      {
         #ifndef CUSTOM_CC
            if (self->m_iFlowWindowSize <= ((self->m_iSndCurrSeqNo - self->m_iSndLastAck + 1 + self->m_iMaxSeqNo) % self->m_iMaxSeqNo))
         #else
            cwnd = (self->m_iFlowWindowSize < (__int32)self->m_dCongestionWindow) ? self->m_iFlowWindowSize : (__int32)self->m_dCongestionWindow;
            if (cwnd <= ((self->m_iSndCurrSeqNo - self->m_iSndLastAck + 1 + self->m_iMaxSeqNo) % self->m_iMaxSeqNo))
         #endif
         {
            //wait here for ACK, NAK, or EXP (i.e, some data to sent)
            #ifndef WIN32
               gettimeofday(&now, 0);
               if (now.tv_usec < 990000)
               {
                  timeout.tv_sec = now.tv_sec;
                  timeout.tv_nsec = (now.tv_usec + 10000) * 1000;
               }
               else
               {
                  timeout.tv_sec = now.tv_sec + 1;
                  timeout.tv_nsec = now.tv_usec * 1000;
               }
               pthread_cond_timedwait(&self->m_WindowCond, &self->m_WindowLock, &timeout);
            #else
               WaitForSingleObject(self->m_WindowCond, 1);
            #endif

            self->m_pSndTimeWindow->onPktSndInt();

            #ifdef NO_BUSY_WAITING
               // the waiting time should not be counted in. clear the time diff to zero.
               self->m_ullTimeDiff = 0;
            #endif

            continue;
         }

         if (0 == (payload = self->m_pSndBuffer->readData(&(datapkt.m_pcData), self->m_iPayloadSize)))
         {
            //check if the sender buffer is empty
            if (0 == self->m_pSndBuffer->getCurrBufSize())
            {
               // If yes, sleep here until a signal comes.
               #ifndef WIN32
                  pthread_mutex_lock(&(self->m_SendDataLock));
                  while ((0 == self->m_pSndBuffer->getCurrBufSize()) && (!self->m_bClosing))
                     pthread_cond_wait(&(self->m_SendDataCond), &(self->m_SendDataLock));
                  pthread_mutex_unlock(&(self->m_SendDataLock));
               #else
                  WaitForSingleObject(self->m_SendDataLock, INFINITE);
                  while ((0 == self->m_pSndBuffer->getCurrBufSize()) && (!self->m_bClosing))
                  {
                     ReleaseMutex(self->m_SendDataLock);
                     WaitForSingleObject(self->m_SendDataCond, INFINITE);
                     WaitForSingleObject(self->m_SendDataLock, INFINITE);
                  }
                  ReleaseMutex(self->m_SendDataLock);
               #endif
            }

            self->m_pSndTimeWindow->onPktSndInt();

            #ifdef NO_BUSY_WAITING
               // the waiting time should not be counted in. clear the time diff to zero.
               self->m_ullTimeDiff = 0;
            #endif

            continue;
         }

         self->m_iSndCurrSeqNo = (self->m_iSndCurrSeqNo + 1) % self->m_iMaxSeqNo;
         datapkt.m_iSeqNo = self->m_iSndCurrSeqNo;
 
         if (0 == self->m_iSndCurrSeqNo % self->m_iProbeInterval)
            probe = true;
      }

      // Now sending.
      datapkt.setLength(payload);
      *(self->m_pChannel) << datapkt;

      self->m_pSndTimeWindow->onPktSent();

      #ifdef CUSTOM_CC
         if (NULL != self->m_pCC)
            self->m_pCC->onPktSent(datapkt.m_iSeqNo, datapkt.getLength());
      #endif

      #ifdef TRACE
         ++ self->m_llTraceSent;
      #endif

      if (probe)
      {
         // sends out probing packet pair
         self->m_pTimer->rdtsc(targettime);
         probe = false;
      }
      else if (self->m_bFreeze)
      {
         // sending is fronzen!
         targettime = entertime + self->m_iSYNInterval * self->m_ullCPUFrequency + self->m_ullInterval;
         self->m_bFreeze = false;
      }
      else
         targettime = entertime + self->m_ullInterval;

      // wait for an inter-packet time.
      #ifndef NO_BUSY_WAITING
         self->m_pTimer->sleepto(targettime);
      #else
         self->m_pTimer->rdtsc(currtime);

         if (currtime >= targettime)
            continue;

         while (currtime + self->m_ullTimeDiff < targettime)
         {
            #ifndef WIN32
               gettimeofday(&now, 0);
               if (now.tv_usec < 990000)
               {
                  timeout.tv_sec = now.tv_sec;
                  timeout.tv_nsec = (now.tv_usec + 10000) * 1000;
               }
               else
               {
                  timeout.tv_sec = now.tv_sec + 1;
                  timeout.tv_nsec = now.tv_usec * 1000;
               }
               if (0 == pthread_cond_timedwait(&self->m_WindowCond, &self->m_WindowLock, &timeout))
                  break;
            #else
               if (WAIT_TIMEOUT != WaitForSingleObject(self->m_WindowCond, 1))
                  break;
            #endif
            self->m_pTimer->rdtsc(currtime);
         }

         self->m_pTimer->rdtsc(currtime);
         if (currtime >= targettime)
            self->m_ullTimeDiff += currtime - targettime;
         else
            self->m_ullTimeDiff -= targettime - currtime;
      #endif
   }

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

#ifndef WIN32
void* CUDT::rcvHandler(void* recver)
#else
DWORD WINAPI CUDT::rcvHandler(LPVOID recver)
#endif
{
   CUDT* self = static_cast<CUDT *>(recver);

   CPacket packet;
   char* payload = new char [self->m_iPayloadSize];
   bool nextslotfound;
   __int32 offset;
   __int32 loss;
   #ifdef CUSTOM_CC
      __int32 pktcount = 0;
      const __int32 ackint = self->m_pCC->m_iACKInterval;
      bool gotdata = false;
   #endif

   // time
   unsigned __int64 currtime;
   unsigned __int64 nextacktime;
   unsigned __int64 nextnaktime;
   unsigned __int64 nextexptime;

   // SYN interval, in clock cycles
   const unsigned __int64 ullsynint = self->m_iSYNInterval * self->m_ullCPUFrequency;

   // ACK, NAK, and EXP intervals, in clock cycles
   unsigned __int64 ullackint = ullsynint;
   #ifdef CUSTOM_CC
      if ((NULL != self->m_pCC) && self->m_pCC->m_bPeriodicalACK)
         ullackint = self->m_pCC->m_iACKPeriod * 1000 * self->m_ullCPUFrequency;
   #endif
   unsigned __int64 ullnakint = (self->m_iRTT + 4 * self->m_iRTTVar) * self->m_ullCPUFrequency;
   unsigned __int64 ullexpint = (self->m_iRTT + 4 * self->m_iRTTVar) * self->m_ullCPUFrequency + ullsynint;

   // Set up the timers.
   self->m_pTimer->rdtsc(nextacktime);
   nextacktime += ullackint;
   self->m_pTimer->rdtsc(nextnaktime);
   nextnaktime += ullnakint;
   self->m_pTimer->rdtsc(nextexptime);
   nextexptime += ullexpint;

   while (!self->m_bClosing)
   {
      #ifdef NO_BUSY_WAITING
         // signal sleeping sender
         #ifndef WIN32
            pthread_cond_signal(&self->m_WindowCond);
         #else
            SetEvent(self->m_WindowCond);
         #endif
      #endif

      #ifdef CUSTOM_CC
         if (NULL != self->m_pCC)
         {
            // update CC parameters
            self->m_ullInterval = (unsigned __int64)(self->m_pCC->m_dPktSndPeriod * self->m_ullCPUFrequency);
            self->m_dCongestionWindow = self->m_pCC->m_dCWndSize;
         }
      #endif

      // "recv"/"recvfile" is called, overlapped mode is activated, and not enough received data in the protocol buffer
      if (self->m_bReadBuf)
      {
         // Check if there is enough data now.
         #ifndef WIN32
            pthread_mutex_lock(&(self->m_OverlappedRecvLock));
            self->m_bReadBuf = self->m_pRcvBuffer->readBuffer(const_cast<char*>(self->m_pcTempData), const_cast<__int32&>(self->m_iTempLen));
            pthread_mutex_unlock(&(self->m_OverlappedRecvLock));
         #else
            WaitForSingleObject(self->m_OverlappedRecvLock, INFINITE);
            self->m_bReadBuf = self->m_pRcvBuffer->readBuffer(const_cast<char*>(self->m_pcTempData), const_cast<__int32&>(self->m_iTempLen));
            ReleaseMutex(self->m_OverlappedRecvLock);
         #endif

         // Still no?! Register the application buffer.
         if (!self->m_bReadBuf)
         {
            offset = self->m_pRcvBuffer->registerUserBuf(const_cast<char*>(self->m_pcTempData), const_cast<__int32&>(self->m_iTempLen), self->m_iRcvHandle, self->m_iTempRoutine);
            // there is no seq. wrap for user buffer border. If it exceeds the max. seq., we just ignore it.
            self->m_iUserBufBorder = self->m_iRcvLastAck + (__int32)ceil(double(self->m_iTempLen - offset) / self->m_iPayloadSize);
         }

         // Otherwise, inform the blocked "recv"/"recvfile" call that the expected data has arrived.
         // or returns immediately in non-blocking IO mode.
         if (self->m_bReadBuf || !self->m_bSynRecving)
         {
            self->m_bReadBuf = false;
            #ifndef WIN32
               pthread_mutex_lock(&(self->m_OverlappedRecvLock));
               pthread_cond_signal(&(self->m_OverlappedRecvCond));
               pthread_mutex_unlock(&(self->m_OverlappedRecvLock));
            #else
               SetEvent(self->m_OverlappedRecvCond);
            #endif
         }
      }

      self->m_pTimer->rdtsc(currtime);
      loss = self->m_pRcvLossList->getFirstLostSeq();

      // Query the timers if any of them is expired.
      if ((currtime > nextacktime) || (loss >= self->m_iUserBufBorder) || ((self->m_iRcvCurrSeqNo >= self->m_iUserBufBorder - 1) && (loss < 0)))
      {
         // ACK timer expired, or user buffer is fulfilled.
         self->sendCtrl(2);

         self->m_pTimer->rdtsc(currtime);
         nextacktime = currtime + ullackint;
      }
      #ifdef CUSTOM_CC
         //send a "light" ACK
         else if (gotdata && (0 == ++ pktcount % ackint))
         {
            self->sendCtrl(2, NULL, NULL, 2 * sizeof(__int32));
            pktcount = 0;
         }

         gotdata = false;
      #endif

      if ((loss >= 0) && (currtime > nextnaktime))
      {
         // NAK timer expired, and there is loss to be reported.
         self->sendCtrl(3);

         self->m_pTimer->rdtsc(currtime);
         nextnaktime = currtime + ullnakint;
      }
      else if ((currtime > nextexptime) && (0 == self->m_pSndLossList->getLossLength()))
      {
         // Haven't receive any information from the peer, is it dead?!
         // timeout: at least 16 expirations and must be greater than 3 seconds and be less than 30 seconds
         if (((self->m_iEXPCount > 16) && 
             (self->m_iEXPCount * ((self->m_iEXPCount - 1) * (self->m_iRTT + 4 * self->m_iRTTVar) / 2 + self->m_iSYNInterval) > 3000000))
             || (self->m_iEXPCount * ((self->m_iEXPCount - 1) * (self->m_iRTT + 4 * self->m_iRTTVar) / 2 + self->m_iSYNInterval) > 30000000))
         {
            //
            // Connection is broken. 
            // UDT does not signal any information about this instead of to stop quietly.
            // Apllication will detect this when it calls any UDT methods next time.
            //
            self->m_bClosing = true;
            self->m_bBroken = true;

            self->releaseSynch();

            continue;
         }

         #ifdef CUSTOM_CC
            if (NULL != self->m_pCC)
               self->m_pCC->onTimeout();
         #endif

         // sender: Insert all the packets sent after last received acknowledgement into the sender loss list.
         // recver: Send out a keep-alive packet
         if (((self->m_iSndCurrSeqNo + 1) % self->m_iMaxSeqNo) != self->m_iSndLastAck)
         {
            __int32 csn = self->m_iSndCurrSeqNo;
            self->m_pSndLossList->insert(const_cast<__int32&>(self->m_iSndLastAck), csn);
         }
         else
            self->sendCtrl(1);

         if (self->m_pSndBuffer->getCurrBufSize() > 0)
         {
            // Wake up the waiting sender (avoiding deadlock on an infinite sleeping)
            self->m_pTimer->interrupt();

            #ifndef WIN32
               pthread_cond_signal(&self->m_WindowCond);
            #else
               SetEvent(self->m_WindowCond);
            #endif
         }

         ++ self->m_iEXPCount;

         ullexpint = (self->m_iEXPCount * (self->m_iRTT + 4 * self->m_iRTTVar) + self->m_iSYNInterval) * self->m_ullCPUFrequency;

         self->m_pTimer->rdtsc(nextexptime);
         nextexptime += ullexpint;
      }

      ////////////////////////////////////////////////////////////////////////////////////////////
      // Below is the packet receiving/processing part.

      packet.setLength(self->m_iPayloadSize);

      offset = self->m_iNextExpect - self->m_iRcvLastAck;
      if (offset < -self->m_iSeqNoTH)
         offset += self->m_iMaxSeqNo;

      // Look for a slot for the speculated data.
      if (!(self->m_pRcvBuffer->nextDataPos(&(packet.m_pcData), offset * self->m_iPayloadSize - self->m_pIrrPktList->currErrorSize(self->m_iNextExpect), self->m_iPayloadSize)))
      {
         packet.m_pcData = payload;
         nextslotfound = false;
      }
      else
         nextslotfound = true;

      // Receiving...
      *(self->m_pChannel) >> packet;

      // Got nothing?
      if (packet.getLength() <= 0)
         continue;

      // Just heard from the peer, reset the expiration count.
      self->m_iEXPCount = 1;
      ullexpint = (self->m_iRTT + 4 * self->m_iRTTVar) * self->m_ullCPUFrequency + ullsynint;
      if (((self->m_iSndCurrSeqNo + 1) % self->m_iMaxSeqNo) == self->m_iSndLastAck)
      {
         self->m_pTimer->rdtsc(nextexptime);
         nextexptime += ullexpint;
      }

      // But this is control packet, process it!
      if (packet.getFlag())
      {
         self->processCtrl(packet);

         if ((2 == packet.getType()) || (6 == packet.getType()))
         {
            ullnakint = (self->m_iRTT + 4 * self->m_iRTTVar) * self->m_ullCPUFrequency;
            //do not resent the loss report within too short period
            if (ullnakint < ullsynint)
               ullnakint = ullsynint;
         }

         self->m_pTimer->rdtsc(currtime);
         if ((2 <= packet.getType()) && (4 >= packet.getType()))
            nextexptime = currtime + ullexpint;

         continue;
      }

      // update time/delay information
      self->m_pRcvTimeWindow->onPktArrival();

      // check if it is probing packet pair
      if (packet.m_iSeqNo % self->m_iProbeInterval < 2)
      {
         if (0 == packet.m_iSeqNo % self->m_iProbeInterval)
            self->m_pRcvTimeWindow->probe1Arrival();
         else
            self->m_pRcvTimeWindow->probe2Arrival();
      }

      #ifdef TRACE
         ++ self->m_llTraceRecv;
      #endif

      offset = packet.m_iSeqNo - self->m_iRcvLastAck;
      if (offset < -self->m_iSeqNoTH)
         offset += self->m_iMaxSeqNo;

      // Data is too old, discard it!
      if ((offset >= self->m_iFlightFlagSize) || (offset < 0))
         continue;

      // Oops, the speculation is wrong...
      if ((packet.m_iSeqNo != self->m_iNextExpect) || (!nextslotfound))
      {
         // Put the received data explicitly into the right slot.
         if (!(self->m_pRcvBuffer->addData(packet.m_pcData, offset * self->m_iPayloadSize - self->m_pIrrPktList->currErrorSize(packet.m_iSeqNo), packet.getLength())))
            continue;

         // Loss detection.
         if (((packet.m_iSeqNo > self->m_iRcvCurrSeqNo + 1) && (packet.m_iSeqNo - self->m_iRcvCurrSeqNo < self->m_iSeqNoTH)) || (packet.m_iSeqNo < self->m_iRcvCurrSeqNo - self->m_iSeqNoTH))
         {
            // If loss found, insert them to the receiver loss list
            self->m_pRcvLossList->insert(self->m_iRcvCurrSeqNo + 1, packet.m_iSeqNo - 1);

            // pack loss list for NAK
            __int32 lossdata[2];
            lossdata[0] = (self->m_iRcvCurrSeqNo + 1) | 0x80000000;
            lossdata[1] = packet.m_iSeqNo - 1;

            // Generate loss report immediately.
            self->sendCtrl(3, NULL, lossdata, (self->m_iRcvCurrSeqNo + 1 == packet.m_iSeqNo - 1) ? 1 : 2);

            #ifdef TRACE
               ++ self->m_iTraceLoss;
            #endif
         }
      }

      // This is not a regular fixed size packet...
      if (packet.getLength() != self->m_iPayloadSize)
         self->m_pIrrPktList->addIrregularPkt(packet.m_iSeqNo, self->m_iPayloadSize - packet.getLength());

      // Update the current largest sequence number that has been received.
      if (((packet.m_iSeqNo > self->m_iRcvCurrSeqNo) && (packet.m_iSeqNo - self->m_iRcvCurrSeqNo < self->m_iSeqNoTH)) || (packet.m_iSeqNo < self->m_iRcvCurrSeqNo - self->m_iSeqNoTH))
      {
         self->m_iRcvCurrSeqNo = packet.m_iSeqNo;

         // Speculate next packet.
         self->m_iNextExpect = (self->m_iRcvCurrSeqNo + 1) % self->m_iMaxSeqNo;
      }
      else
      {
         // Or it is a retransmitted packet, remove it from receiver loss list.
         // rearrange receiver buffer if it is a first-come irregular packet

         if (self->m_pRcvLossList->remove(packet.m_iSeqNo) && (packet.getLength() < self->m_iPayloadSize))
            self->m_pRcvBuffer->moveData(offset * self->m_iPayloadSize - self->m_pIrrPktList->currErrorSize(packet.m_iSeqNo) + packet.getLength(), self->m_iPayloadSize - packet.getLength());
      }

      #ifdef CUSTOM_CC
         if (NULL != self->m_pCC)
            self->m_pCC->onPktReceived(packet.m_iSeqNo, packet.getLength());

         gotdata = true;
      #endif
   }

   delete [] payload;

   #ifndef WIN32
      return NULL;
   #else
      return 0;
   #endif
}

void CUDT::sendCtrl(const __int32& pkttype, void* lparam, void* rparam, const __int32& size)
{
   CPacket ctrlpkt;

   switch (pkttype)
   {
   case 2: //010 - Acknowledgement
      {
      __int32 ack;

      // If there is no loss, the ACK is the current largest sequence number plus 1;
      // Otherwise it is the smallest sequence number in the receiver loss list.
      if (0 == m_pRcvLossList->getLossLength())
         ack = (m_iRcvCurrSeqNo + 1) % m_iMaxSeqNo;
      else
         ack = m_pRcvLossList->getFirstLostSeq();

      #ifdef CUSTOM_CC
         // send out a lite ACK
         // to save time on buffer processing and bandwidth/AS measurement, a lite ACK only feeds back an ACK number
         if (size == 2 * sizeof(__int32))
         {
            ctrlpkt.pack(2, NULL, &ack, 2 * sizeof(__int32));
            *m_pChannel << ctrlpkt;
               
            break;
         }
      #endif

      unsigned __int64 currtime;
      m_pTimer->rdtsc(currtime);

      // There is new received packet to acknowledge, update related information.
      if (((ack > m_iRcvLastAck) && (ack - m_iRcvLastAck < m_iSeqNoTH)) || (ack < m_iRcvLastAck - m_iSeqNoTH))
      {
         int acksize = (ack - m_iRcvLastAck + m_iMaxSeqNo) % m_iMaxSeqNo;
         m_iRcvLastAck = ack;

         if (m_pRcvBuffer->ackData(acksize * m_iPayloadSize - m_pIrrPktList->currErrorSize(m_iRcvLastAck)))
         {
            //singal an blocking overlapped IO. 
            #ifndef WIN32
               pthread_mutex_lock(&m_OverlappedRecvLock);
               pthread_cond_signal(&m_OverlappedRecvCond);
               pthread_mutex_unlock(&m_OverlappedRecvLock);
            #else
               SetEvent(m_OverlappedRecvCond);
            #endif
         }

         m_iUserBufBorder = m_iRcvLastAck + (__int32)ceil(double(m_pRcvBuffer->getAvailBufSize()) / m_iPayloadSize);

         // signal a waiting "recv" call if there is any data available
         #ifndef WIN32
            pthread_mutex_lock(&m_RecvDataLock);
            if ((m_bSynRecving) && (0 != m_pRcvBuffer->getRcvDataSize()))
               pthread_cond_signal(&m_RecvDataCond);
            pthread_mutex_unlock(&m_RecvDataLock);
         #else
            if ((m_bSynRecving) && (0 != m_pRcvBuffer->getRcvDataSize()))
               SetEvent(m_RecvDataCond);
         #endif

         m_pIrrPktList->deleteIrregularPkt(m_iRcvLastAck);
      }
      else if (ack == m_iRcvLastAck)
      {
         if ((currtime - m_ullLastAckTime) < ((m_iRTT + 4 * m_iRTTVar) * m_ullCPUFrequency))
            break;
      }
      else
         break;

      // Send out the ACK only if has not been received by the sender before
      if (((m_iRcvLastAck > m_iRcvLastAckAck) && (m_iRcvLastAck - m_iRcvLastAckAck < m_iSeqNoTH)) || (m_iRcvLastAck < m_iRcvLastAckAck - m_iSeqNoTH))
      {
         __int32* data = new __int32 [5];

         m_iAckSeqNo = (m_iAckSeqNo + 1) % m_iMaxAckSeqNo;
         data[0] = m_iRcvLastAck;
         data[1] = m_iRTT;
         data[2] = m_iRTTVar;

         flowControl(m_pRcvTimeWindow->getPktRcvSpeed());
         data[3] = m_iFlowControlWindow;
         if (data[3] > (__int32)(m_pRcvBuffer->getAvailBufSize() / m_iPayloadSize))
            data[3] = (__int32)(m_pRcvBuffer->getAvailBufSize() / m_iPayloadSize);
         if (data[3] < 2)
            data[3] = 2;

         data[4] = m_bRcvSlowStart? 0 : m_pRcvTimeWindow->getBandwidth();

         ctrlpkt.pack(2, &m_iAckSeqNo, data, 5 * sizeof(__int32));
         *m_pChannel << ctrlpkt;

         m_pACKWindow->store(m_iAckSeqNo, m_iRcvLastAck);

         m_pTimer->rdtsc(m_ullLastAckTime);

         #ifdef TRACE
            ++ m_iSentACK;
         #endif

         delete [] data;
      }

      break;
      }

   case 6: //110 - Acknowledgement of Acknowledgement
      ctrlpkt.pack(6, lparam);

      *m_pChannel << ctrlpkt;

      break;

   case 3: //011 - Loss Report
      if (NULL != rparam)
      {
         if (1 == size)
         {
            // only 1 loss packet
            ctrlpkt.pack(3, NULL, (__int32 *)rparam + 1, sizeof(__int32));
         }
         else
         {
            // more than 1 loss packets
            ctrlpkt.pack(3, NULL, rparam, 2 * sizeof(__int32));
         }

         *m_pChannel << ctrlpkt;

         //Slow Start Stopped, if it is not
         m_bRcvSlowStart = false;

         #ifdef TRACE
            ++ m_iSentNAK;
         #endif
      }
      else if (m_pRcvLossList->getLossLength() > 0)
      {
         // this is periodically NAK report

         // read loss list from the local receiver loss list
         __int32* data = new __int32 [m_iPayloadSize];
         __int32 losslen;
         m_pRcvLossList->getLossArray(data, losslen, m_iPayloadSize / sizeof(__int32), m_iRTT + 4 * m_iRTTVar);

         if (0 < losslen)
         {
            ctrlpkt.pack(3, NULL, data, losslen * sizeof(__int32));
            *m_pChannel << ctrlpkt;

            //Slow Start Stopped, if it is not
            m_bRcvSlowStart = false;

            #ifdef TRACE
               ++ m_iSentNAK;
            #endif
         }

         delete [] data;
      }

      break;

   case 4: //100 - Congestion Warning
      ctrlpkt.pack(4);
      *m_pChannel << ctrlpkt;

      //Slow Start Stopped, if it is not
      m_bRcvSlowStart = false;

      m_pTimer->rdtsc(m_ullLastWarningTime);

      break;

   case 1: //001 - Keep-alive
      ctrlpkt.pack(1);
      *m_pChannel << ctrlpkt;
      
      break;

   case 0: //000 - Handshake
      ctrlpkt.pack(0, NULL, rparam, sizeof(CHandShake));
      *m_pChannel << ctrlpkt;

      break;

   case 5: //101 - Shutdown
      ctrlpkt.pack(5);
      *m_pChannel << ctrlpkt;

      break;

   case 7: //111 - Resevered for future use
      break;

   default:
      break;
   }
}

void CUDT::processCtrl(CPacket& ctrlpkt)
{
   switch (ctrlpkt.getType())
   {
   case 2: //010 - Acknowledgement
      {
      __int32 ack;

      #ifdef CUSTOM_CC
         if (NULL != m_pCC)
            m_pCC->onACK(*(__int32 *)ctrlpkt.m_pcData);

         // process a lite ACK
         if (ctrlpkt.getLength() == 2 * sizeof(__int32))
         {
            ack = *(__int32 *)ctrlpkt.m_pcData;
            if (((ack > m_iSndLastAck) && (ack - m_iSndLastAck < m_iSeqNoTH)) || (ack < m_iSndLastAck - m_iSeqNoTH))
               m_iSndLastAck = ack;

            #ifndef WIN32
               pthread_cond_signal(&m_WindowCond);
            #else
               SetEvent(m_WindowCond);
            #endif

            break;
         }
      #endif

      // read ACK seq. no.
      ack = ctrlpkt.getAckSeqNo();

      // send ACK acknowledgement
      if (ctrlpkt.getLength() == 5 * sizeof(__int32))
         sendCtrl(6, &ack);

      // Got data ACK
      ack = *(__int32 *)ctrlpkt.m_pcData;

      if (((ack > m_iSndLastAck) && (ack - m_iSndLastAck < m_iSeqNoTH)) || (ack < m_iSndLastAck - m_iSeqNoTH))
         m_iSndLastAck = ack;

      // protect packet retransmission
      #ifndef WIN32
         pthread_mutex_lock(&m_AckLock);
      #else
         WaitForSingleObject(m_AckLock, INFINITE);
      #endif

      // acknowledge the sending buffer
      if ((ack > m_iSndLastDataAck) && (ack - m_iSndLastDataAck < m_iSeqNoTH))
         m_pSndBuffer->ackData((ack - m_iSndLastDataAck) * m_iPayloadSize, m_iPayloadSize);
      else if (ack < m_iSndLastDataAck - m_iSeqNoTH)
         m_pSndBuffer->ackData((ack - m_iSndLastDataAck + m_iMaxSeqNo) * m_iPayloadSize, m_iPayloadSize);
      else
      {
         // discard it if it is a repeated ACK
         #ifndef WIN32
            pthread_mutex_unlock(&m_AckLock);
         #else
            ReleaseMutex(m_AckLock);
         #endif

         break;
      }

      // update sending variables
      m_iSndLastDataAck = ack;
      m_pSndLossList->remove((m_iSndLastDataAck - 1 + m_iMaxSeqNo) % m_iMaxSeqNo);

      #ifndef WIN32
         pthread_mutex_unlock(&m_AckLock);

         pthread_cond_signal(&m_WindowCond);

         pthread_mutex_lock(&m_SendBlockLock);
         if (m_bSynSending)
            pthread_cond_signal(&m_SendBlockCond);
         pthread_mutex_unlock(&m_SendBlockLock);
      #else
         ReleaseMutex(m_AckLock);

         SetEvent(m_WindowCond);

         if (m_bSynSending)
            SetEvent(m_SendBlockCond);
      #endif

      // Update RTT
      m_iRTT = *((__int32 *)ctrlpkt.m_pcData + 1);
      m_iRTTVar = *((__int32 *)ctrlpkt.m_pcData + 2);

      // Update Flow Window Size
      m_iFlowWindowSize = *((__int32 *)ctrlpkt.m_pcData + 3);

      #ifndef CUSTOM_CC
         // quick start
         if ((m_bSndSlowStart) && (*((__int32 *)ctrlpkt.m_pcData + 4) > 0))
         {
            m_bSndSlowStart = false;
            m_ullInterval = m_iFlowWindowSize * m_ullCPUFrequency / (m_iRTT + m_iSYNInterval);
         }
      #endif

      // Update Estimated Bandwidth
      if (*((__int32 *)ctrlpkt.m_pcData + 4) > 0)
         m_iBandwidth = (m_iBandwidth * 7 + *((__int32 *)ctrlpkt.m_pcData + 4)) >> 3;

      #ifndef CUSTOM_CC
         // an ACK may activate rate control
         timeval currtime;
         gettimeofday(&currtime, 0);

         if (((currtime.tv_sec - m_LastSYNTime.tv_sec) * 1000000 + currtime.tv_usec - m_LastSYNTime.tv_usec) >= m_iSYNInterval)
         {
            m_LastSYNTime = currtime;

            rateControl();
         }
      #endif

      // Wake up the waiting sender and correct the sending rate
      m_pTimer->interrupt();

      #ifdef TRACE
         ++ m_iRecvACK;
      #endif

      break;
      }

   case 6: //110 - Acknowledgement of Acknowledgement
      {
      __int32 ack;
      __int32 rtt = -1;
      //timeval currtime;

      // update RTT
      rtt = m_pACKWindow->acknowledge(ctrlpkt.getAckSeqNo(), ack);

      if (rtt <= 0)
         break;

      //
      // Well, I decide to temporaly disable the use of delay.
      // a good idea but the algorithm to detect it is not good enough.
      // I'll come back later...
      //

      //m_pRcvTimeWindow->ack2Arrival(rtt);

      // check packet delay trend
      //m_pTimer->rdtsc(currtime);
      //if (m_pRcvTimeWindow->getDelayTrend() && (currtime - m_ullLastWarningTime > (m_iRTT + 4 * m_iRTTVar) * m_ullCPUFrequency))
      //   sendCtrl(4);

      // RTT EWMA
      m_iRTTVar = (m_iRTTVar * 3 + abs(rtt - m_iRTT)) >> 2;
      m_iRTT = (m_iRTT * 7 + rtt) >> 3;

      // update last ACK that has been received by the sender
      if (((m_iRcvLastAckAck < ack) && (ack - m_iRcvLastAckAck < m_iSeqNoTH)) || (m_iRcvLastAckAck > ack + m_iSeqNoTH))
         m_iRcvLastAckAck = ack;

      break;
      }

   case 3: //011 - Loss Report
      {
      #ifndef CUSTOM_CC
         //Slow Start Stopped, if it is not
         m_bSndSlowStart = false;
      #endif

      __int32* losslist = (__int32 *)(ctrlpkt.m_pcData);

      #ifndef CUSTOM_CC
         // Congestion Control on Loss
         if ((((losslist[0] & 0x7FFFFFFF) > m_iLastDecSeq) && ((losslist[0] & 0x7FFFFFFF) - m_iLastDecSeq < m_iSeqNoTH)) || ((losslist[0] & 0x7FFFFFFF) < m_iLastDecSeq - m_iSeqNoTH))
         {
            m_bFreeze = true;

            m_ullLastDecRate = m_ullInterval;
            m_ullInterval = (unsigned __int64)ceil(m_ullInterval * 1.125);

            m_iAvgNAKNum = (__int32)ceil((double)m_iAvgNAKNum * 0.875 + (double)m_iNAKCount * 0.125) + 1;
            m_iNAKCount = 1;

            m_iLastDecSeq = m_iSndCurrSeqNo;

            // remove global synchronization using randomization
            srand(m_iLastDecSeq);
            m_iDecRandom = (__int32)(rand() * double(m_iAvgNAKNum) / (RAND_MAX + 1.0)) + 1;
         }
         else if (0 == (++ m_iNAKCount % m_iDecRandom))
         {
            m_ullInterval = (unsigned __int64)ceil(m_ullInterval * 1.125);

            m_iLastDecSeq = m_iSndCurrSeqNo;
         }
      #else
         if (NULL != m_pCC)
            m_pCC->onLoss(losslist, ctrlpkt.getLength());
      #endif

      // decode loss list message and insert loss into the sender loss list
      for (__int32 i = 0, n = (__int32)(ctrlpkt.getLength() / sizeof(__int32)); i < n; ++ i)
      {
         if (0 != (losslist[i] & 0x80000000))
         {
            if (((losslist[i] & 0x7FFFFFFF) >= m_iSndLastAck) || ((losslist[i] & 0x7FFFFFFF) < m_iSndLastAck - m_iSeqNoTH))
            {
               #ifdef TRACE
                  m_iTraceLoss += m_pSndLossList->insert(losslist[i] & 0x7FFFFFFF, losslist[i + 1]);
               #else
                  m_pSndLossList->insert(losslist[i] & 0x7FFFFFFF, losslist[i + 1]);
               #endif
               ++ i;
            }
         }
         else if ((losslist[i] >= m_iSndLastAck) || (losslist[i] < m_iSndLastAck - m_iSeqNoTH))
         {
            #ifdef TRACE
               m_iTraceLoss += m_pSndLossList->insert(losslist[i], losslist[i]);
            #else
               m_pSndLossList->insert(losslist[i], losslist[i]);
            #endif
         }
      }

      // Wake up the waiting sender (avoiding deadlock on an infinite sleeping)
      m_pSndLossList->insert(const_cast<__int32&>(m_iSndLastAck), const_cast<__int32&>(m_iSndLastAck));
      m_pTimer->interrupt();

      #ifndef WIN32
         pthread_cond_signal(&m_WindowCond);
      #else
         SetEvent(m_WindowCond);
      #endif

      // loss received during this SYN
      m_bLoss = true;

      #ifdef TRACE
         ++ m_iRecvNAK;
      #endif

      break;
      }

   case 4: //100 - Delay Warning
      #ifndef CUSTOM_CC
         //Slow Start Stopped, if it is not
         m_bSndSlowStart = false;

         // One way packet delay is increasing, so decrease the sending rate
         m_ullInterval = (unsigned __int64)ceil(m_ullInterval * 1.125);

         m_iLastDecSeq = m_iSndCurrSeqNo;
      #endif

      break;

   case 1: //001 - Keep-alive
      // The only purpose of keep-alive packet is to tell the peer is still alive
      // nothing need to be done.

      break;

   case 0: //000 - Handshake
      if ((m_bInitiator) && (m_iPeerISN - 1 == m_iRcvCurrSeqNo) && (m_iISN == m_iSndLastAck))
      {
         // The peer side has not received the handshake message, so it keeping query
         // resend the handshake packet

         CHandShake initdata;
         initdata.m_iISN = m_iISN;
         initdata.m_iMSS = m_iMSS;
         initdata.m_iFlightFlagSize = m_iFlightFlagSize;
         sendCtrl(0, NULL, (char *)&initdata, sizeof(CHandShake));
      }

      // I am not an initiator, so both the initiator and I must have received the message before I came here

      break;

   case 5: //101 - Shutdown
      m_bShutdown = true;
      m_bClosing = true;
      m_bBroken = true;

      // Signal the sender and recver if they are waiting for data.
      releaseSynch();

      break;

   case 7: //111 - reserved and user defined messages
      #ifdef CUSTOM_CC
         if (NULL != m_pCC)
            m_pCC->processCustomMsg(ctrlpkt);
      #endif

      break;

   default:
      break;
   }
}

void CUDT::rateControl()
{
   // During Slow Start, no rate increase
   if (m_bSndSlowStart)
      return;

   if (m_bLoss)
   {
      m_bLoss = false;
      return;
   }

   __int32 B = __int32(m_iBandwidth - 1000000.0 / m_ullInterval * m_ullCPUFrequency);
   if ((m_ullInterval > m_ullLastDecRate) && ((m_iBandwidth / 9) < B))
      B = m_iBandwidth / 9;

   double inc;

   if (B <= 0)
      inc = 1.0 / m_iMSS;
   else
   {
      // inc = max(10 ^ ceil(log10( B * MSS * 8 ) * Beta / MSS, 1/MSS)
      // Beta = 1.5 * 10^(-6)

      inc = pow(10.0, ceil(log10(B * m_iMSS * 8.0))) * 0.0000015 / m_iMSS;

      if (inc < 1.0/m_iMSS)
         inc = 1.0/m_iMSS;
   }

   //correct the sending rate
   unsigned __int64 realrate = (unsigned __int64)(1000000. / m_pSndTimeWindow->getPktSndSpeed() * m_ullCPUFrequency);
   if (realrate >= 2 * m_ullInterval)
      m_ullInterval = realrate / 2;

   m_ullInterval = (unsigned __int64)((m_ullInterval * m_iSYNInterval * m_ullCPUFrequency) / (m_ullInterval * inc + m_iSYNInterval * m_ullCPUFrequency));

   if (m_ullInterval < m_ullCPUFrequency)
      m_ullInterval = m_ullCPUFrequency;
}

void CUDT::flowControl(const __int32& recvrate)
{
   if (m_bRcvSlowStart)
   {
      m_iFlowControlWindow = (m_iRcvLastAck > m_iPeerISN) ? (m_iRcvLastAck - m_iPeerISN) : (m_iRcvLastAck - m_iPeerISN + m_iMaxSeqNo);

      if ((recvrate > 0) && (m_iFlowControlWindow >= m_iQuickStartPkts))
      {
         // quick start
         m_bRcvSlowStart = false;
         m_iFlowControlWindow = (__int32)((__int64)recvrate * (m_iRTT + m_iSYNInterval) / 1000000) + 16;
      }
   }
   else if (recvrate > 0)
      m_iFlowControlWindow = (__int32)ceil(m_iFlowControlWindow * 0.875 + recvrate / 1000000.0 * (m_iRTT + m_iSYNInterval) * 0.125) + 16;

   if (m_iFlowControlWindow > m_iFlightFlagSize)
   {
      m_iFlowControlWindow = m_iFlightFlagSize;
      m_bRcvSlowStart = false;
   }
}

__int32 CUDT::send(char* data, const __int32& len, __int32* overlapped, const UDT_MEM_ROUTINE func)
{
   CGuard sendguard(m_SendLock);

   // throw an exception if not connected
   if (m_bBroken)
      throw CUDTException(2, 1, 0);
   else if (!m_bConnected)
      throw CUDTException(2, 2, 0);

   if (len <= 0)
      return 0;

   // lazy snd thread creation
   #ifndef WIN32
      if (!m_bSndThrStart)
   #else
      if (NULL == m_SndThread)
   #endif
   {
      m_pSndTimeWindow = new CPktTimeWindow();

      #ifndef WIN32
         if (0 != pthread_create(&m_SndThread, NULL, CUDT::sndHandler, this))
            throw CUDTException(7, 0, errno);
         m_bSndThrStart = true;
      #else
         if (NULL == (m_SndThread = CreateThread(NULL, 0, CUDT::sndHandler, this, 0, NULL)))
            throw CUDTException(7, 0, GetLastError());
      #endif
   }

   if (m_pSndBuffer->getCurrBufSize() > m_iSndQueueLimit)
   {
      if (!m_bSynSending)
         throw CUDTException(6, 1, 0);
      else
      {
         // wait here during a blocking sending
         #ifndef WIN32
            pthread_mutex_lock(&m_SendBlockLock);
            while (!m_bBroken && m_bConnected && (m_iSndQueueLimit < m_pSndBuffer->getCurrBufSize()))
               pthread_cond_wait(&m_SendBlockCond, &m_SendBlockLock);
            pthread_mutex_unlock(&m_SendBlockLock);
         #else
            while (!m_bBroken && m_bConnected && (m_iSndQueueLimit < m_pSndBuffer->getCurrBufSize()))
               WaitForSingleObject(m_SendBlockCond, INFINITE);
         #endif

         // check the connection status
         if (m_bBroken)
            throw CUDTException(2, 1, 0);
      }
   }

   char* buf;
   __int32 handle = 0;
   UDT_MEM_ROUTINE r = func;

   if (NULL == overlapped)
   {
      buf = new char[len];
      memcpy(buf, data, len);
      data = buf;
      r = CSndBuffer::releaseBuffer;
   }
   else
   {
      #ifndef WIN32
         pthread_mutex_lock(&m_HandleLock);
      #else
         WaitForSingleObject(m_HandleLock, INFINITE);
      #endif
      if (1 == m_iSndHandle)
         m_iSndHandle = 1 << 30;
      // "send" handle descriptor is POSITIVE and DECREASING
      *overlapped = handle = -- m_iSndHandle;
      #ifndef WIN32
         pthread_mutex_unlock(&m_HandleLock);
      #else
         ReleaseMutex(m_HandleLock);
      #endif
   }

   // insert the user buffer into the sening list
   #ifndef WIN32
      pthread_mutex_lock(&m_SendDataLock);
      m_pSndBuffer->addBuffer(data, len, handle, r);
      pthread_mutex_unlock(&m_SendDataLock);
   #else
      WaitForSingleObject(m_SendDataLock, INFINITE);
      m_pSndBuffer->addBuffer(data, len, handle, r);
      ReleaseMutex(m_SendDataLock);
   #endif

   // signal the sending thread in case that it is waiting
   #ifndef WIN32
      pthread_mutex_lock(&m_SendDataLock);
      pthread_cond_signal(&m_SendDataCond);
      pthread_mutex_unlock(&m_SendDataLock);

      pthread_cond_signal(&m_WindowCond);
   #else
      SetEvent(m_SendDataCond);
      SetEvent(m_WindowCond);
   #endif

   // UDT either sends nothing or sends all 
   return len;
}

__int32 CUDT::recv(char* data, const __int32& len, __int32* overlapped, UDT_MEM_ROUTINE func)
{
   CGuard recvguard(m_RecvLock);

   // throw an exception if not connected
   if (!m_bConnected)
      throw CUDTException(2, 2, 0);
   else if ((m_bBroken) && (0 == m_pRcvBuffer->getRcvDataSize()))
      throw CUDTException(2, 1, 0);
   else if ((m_bSynRecving || (NULL == overlapped)) && (0 < m_pRcvBuffer->getPendingQueueSize()))
      throw CUDTException(6, 4, 0);

   if (len <= 0)
      return 0;

   if ((NULL == overlapped) && (0 == m_pRcvBuffer->getRcvDataSize()))
   {
      if (!m_bSynRecving)
         throw CUDTException(6, 2, 0);
      else
      {
         #ifndef WIN32
            pthread_mutex_lock(&m_RecvDataLock);
            while (!m_bBroken && (0 == m_pRcvBuffer->getRcvDataSize()))
               pthread_cond_wait(&m_RecvDataCond, &m_RecvDataLock);
            pthread_mutex_unlock(&m_RecvDataLock);
         #else
            WaitForSingleObject(m_RecvDataCond, INFINITE);
         #endif
      }
   }

   if ((NULL == overlapped) || (m_bSynRecving && m_bBroken))
   {
      __int32 avail = m_pRcvBuffer->getRcvDataSize();
      if (len <= avail)
         avail = len;

      m_pRcvBuffer->readBuffer(data, avail);
      return avail;
   }

   // Overlapped IO begins.
   if (!m_bSynRecving && m_bBroken)
      throw CUDTException(2, 1, 0);
   else if (m_iUDTBufSize <= m_pRcvBuffer->getPendingQueueSize())
      throw CUDTException(6, 3, 0);

   #ifndef WIN32
      pthread_mutex_lock(&m_OverlappedRecvLock);
   #else
      WaitForSingleObject(m_OverlappedRecvLock, INFINITE);
   #endif

   if (len <= m_pRcvBuffer->getRcvDataSize())
   {
      m_pRcvBuffer->readBuffer(data, len);

      #ifndef WIN32
         pthread_mutex_unlock(&m_OverlappedRecvLock);
      #else
         ReleaseMutex(m_OverlappedRecvLock);
      #endif

      return len;
   }

   m_pcTempData = data;
   m_iTempLen = len;
   m_iTempRoutine = func;
   m_bReadBuf = true;

   #ifndef WIN32
      pthread_mutex_lock(&m_HandleLock);
   #else
      WaitForSingleObject(m_HandleLock, INFINITE);
   #endif
   if (-1 == m_iRcvHandle)
      m_iRcvHandle = -(1 << 30);
   // "recv" handle descriptor is NEGATIVE and INCREASING
   *overlapped = ++ m_iRcvHandle;
   #ifndef WIN32
      pthread_mutex_unlock(&m_HandleLock);
   #else
      ReleaseMutex(m_HandleLock);
   #endif

   #ifndef WIN32
      pthread_cond_wait(&m_OverlappedRecvCond, &m_OverlappedRecvLock);
      pthread_mutex_unlock(&m_OverlappedRecvLock);
   #else
      ReleaseMutex(m_OverlappedRecvLock);
      WaitForSingleObject(m_OverlappedRecvCond, INFINITE);
   #endif

   if (!m_bSynRecving)
      return 0;

   // check if the receiving is successful or the connection is broken
   if (m_bBroken)
   {
      // remove incompleted overlapped recv buffer
      m_pRcvBuffer->removeUserBuf();

      return (len <= m_pRcvBuffer->getRcvDataSize()) ? len : m_pRcvBuffer->getRcvDataSize();
   }

   return len;
}

bool CUDT::getOverlappedResult(const int& handle, __int32& progress, const bool& wait)
{
   // throw an exception if not connected
   if ((m_bBroken) && (0 == m_pRcvBuffer->getRcvDataSize()))
      throw CUDTException(2, 1, 0);
   else if (!m_bConnected)
      throw CUDTException(2, 2, 0);

   // check sending buffer
   if (handle > 0)
   {
      bool res = m_pSndBuffer->getOverlappedResult(handle, progress);
      while (wait && !res && !m_bBroken)
      {
         #ifndef WIN32
            usleep(1);
         #else
            Sleep(1);
         #endif

         res = m_pSndBuffer->getOverlappedResult(handle, progress);
      }
      return res;
   }

   // check receiving buffer
   CGuard recvguard(m_RecvLock);

   bool res = m_pRcvBuffer->getOverlappedResult(handle, progress);
   while (wait && !res && !m_bBroken)
   {
      #ifndef WIN32
         usleep(1);
      #else
         Sleep(1);
      #endif

      res = m_pRcvBuffer->getOverlappedResult(handle, progress);
   }
   return res;
}

__int64 CUDT::sendfile(ifstream& ifs, const __int64& offset, const __int64& size, const __int32& block)
{
   CGuard sendguard(m_SendLock);

   if (m_bBroken)
      throw CUDTException(2, 1, 0);
   else if (!m_bConnected)
      throw CUDTException(2, 2, 0);

   if (size <= 0)
      return 0;

   // lazy snd thread creation
   #ifndef WIN32
      if (!m_bSndThrStart)
   #else
      if (NULL == m_SndThread)
   #endif
   {
      m_pSndTimeWindow = new CPktTimeWindow();

      #ifndef WIN32
         if (0 != pthread_create(&m_SndThread, NULL, CUDT::sndHandler, this))
            throw CUDTException(7, 0, errno);
         m_bSndThrStart = true;
      #else
         if (NULL == (m_SndThread = CreateThread(NULL, 0, CUDT::sndHandler, this, 0, NULL)))
            throw CUDTException(7, 0, GetLastError());
      #endif
   }

   char* tempbuf;
   __int32 unitsize = block;
   __int64 count = 1;

   // positioning...
   try
   {
      ifs.seekg(offset);
   }
   catch (...)
   {
      throw CUDTException(4, 1);
   }

   // sending block by block
   while (unitsize * count <= size)
   {
      tempbuf = new char[unitsize];

      try
      {
         ifs.read(tempbuf, unitsize);
      }
      catch (...)
      {
         throw CUDTException(4, 2);
      }

      #ifndef WIN32
         pthread_mutex_lock(&m_SendDataLock);
         while (!m_bBroken && m_bConnected && (m_pSndBuffer->getCurrBufSize() >= m_iSndQueueLimit))
            usleep(10);
         m_pSndBuffer->addBuffer(tempbuf, unitsize, -1, CSndBuffer::releaseBuffer);
         pthread_cond_signal(&m_SendDataCond);
         pthread_mutex_unlock(&m_SendDataLock);
      #else
         WaitForSingleObject(m_SendDataLock, INFINITE);
         while (!m_bBroken && m_bConnected && (m_pSndBuffer->getCurrBufSize() >= m_iSndQueueLimit))
            Sleep(1);
         m_pSndBuffer->addBuffer(tempbuf, unitsize, -1, CSndBuffer::releaseBuffer);
         SetEvent(m_SendDataCond);
         ReleaseMutex(m_SendDataLock);
      #endif

      if (m_bBroken)
         throw CUDTException(2, 1, 0);

      ++ count;
   }
   if (size - unitsize * (count - 1) > 0)
   {
      tempbuf = new char[size - unitsize * (count - 1)];

      try
      {
         ifs.read(tempbuf, size - unitsize * (count - 1));
      }
      catch (...)
      {
         throw CUDTException(4, 2);
      }

      #ifndef WIN32
         pthread_mutex_lock(&m_SendDataLock);
         while (!m_bBroken && m_bConnected && (m_pSndBuffer->getCurrBufSize() >= m_iSndQueueLimit))
            usleep(10);
         m_pSndBuffer->addBuffer(tempbuf, (__int32)(size - unitsize * (count - 1)), -1, CSndBuffer::releaseBuffer);
         pthread_cond_signal(&m_SendDataCond);
         pthread_mutex_unlock(&m_SendDataLock);
      #else
         WaitForSingleObject(m_SendDataLock, INFINITE);
         while (!m_bBroken && m_bConnected && (m_pSndBuffer->getCurrBufSize() >= m_iSndQueueLimit))
            Sleep(1);
         m_pSndBuffer->addBuffer(tempbuf, (__int32)(size - unitsize * (count - 1)), -1, CSndBuffer::releaseBuffer);
         SetEvent(m_SendDataCond);
         ReleaseMutex(m_SendDataLock);
      #endif

      if (m_bBroken)
         throw CUDTException(2, 1, 0);
   }

   // Wait until all the data is sent out
   while ((!m_bBroken) && m_bConnected && (m_pSndBuffer->getCurrBufSize() > 0))
      #ifndef WIN32
         usleep(10);
      #else
         Sleep(1);
      #endif

   if (m_bBroken && (m_pSndBuffer->getCurrBufSize() > 0))
      throw CUDTException(2, 1, 0);

   return size;
}

__int64 CUDT::recvfile(ofstream& ofs, const __int64& offset, const __int64& size, const __int32& block)
{
   if ((m_bBroken) && (0 == m_pRcvBuffer->getRcvDataSize()))
      throw CUDTException(2, 1, 0);
   else if (!m_bConnected)
      throw CUDTException(2, 2, 0);

   if (size <= 0)
      return 0;

   __int32 unitsize = block;
   __int64 count = 1;
   char* tempbuf = new char[unitsize];
   __int32 recvsize;

   // "recvfile" is always blocking.   
   bool syn = m_bSynRecving;
   m_bSynRecving = true;

   // positioning...
   try
   {
      ofs.seekp(offset);
   }
   catch (...)
   {
      throw CUDTException(4, 3);
   }

   __int32 overlapid;

   // receiving...
   while (unitsize * count <= size)
   {
      try
      {
         recvsize = recv(tempbuf, unitsize, &overlapid);
         ofs.write(tempbuf, recvsize);

         if (recvsize < unitsize)
         {
            m_bSynRecving = syn;
            return unitsize * (count - 1) + recvsize;
         }
      }
      catch (CUDTException e)
      {
         throw e;
      }
      catch (...)
      {
         throw CUDTException(4, 4);
      }

      ++ count;
   }
   if (size - unitsize * (count - 1) > 0)
   {
      try
      {
         recvsize = recv(tempbuf, (__int32)(size - unitsize * (count - 1)), &overlapid);
         ofs.write(tempbuf, recvsize);

         if (recvsize < (__int32)(size - unitsize * (count - 1)))
         {
            m_bSynRecving = syn;
            return unitsize * (count - 1) + recvsize;
         }
      }
      catch (CUDTException e)
      {
         throw e;
      }
      catch (...)
      {
         throw CUDTException(4, 4);
      }
   }

   // recover the original receiving mode
   m_bSynRecving = syn;

   delete [] tempbuf;

   return size;
}

void CUDT::sample(CPerfMon* perf)
{
#ifdef TRACE
   timeval currtime;
   gettimeofday(&currtime, 0);

   perf->msTimeStamp = (currtime.tv_sec - m_StartTime.tv_sec) * 1000 + (currtime.tv_usec - m_StartTime.tv_usec) / 1000;

   m_llSentTotal += m_llTraceSent;
   m_llRecvTotal += m_llTraceRecv;
   m_iLossTotal += m_iTraceLoss;
   m_iRetransTotal += m_iTraceRetrans;
   m_iSentACKTotal += m_iSentACK;
   m_iRecvACKTotal += m_iRecvACK;
   m_iSentNAKTotal += m_iSentNAK;
   m_iRecvNAKTotal += m_iRecvNAK;

   perf->pktSentTotal = m_llSentTotal;
   perf->pktRecvTotal = m_llRecvTotal;
   perf->pktLossTotal = m_iLossTotal;
   perf->pktRetransTotal = m_iRetransTotal;
   perf->pktSentACKTotal = m_iSentACKTotal;
   perf->pktRecvACKTotal = m_iRecvACKTotal;
   perf->pktSentNAKTotal = m_iSentNAKTotal;
   perf->pktRecvNAKTotal = m_iRecvNAKTotal;

   perf->pktSent = m_llTraceSent;
   perf->pktRecv = m_llTraceRecv;
   perf->pktLoss = m_iTraceLoss;
   perf->pktRetrans = m_iTraceRetrans;
   perf->pktSentACK = m_iSentACK;
   perf->pktRecvACK = m_iRecvACK;
   perf->pktSentNAK = m_iSentNAK;
   perf->pktRecvNAK = m_iRecvNAK;

   double interval = (currtime.tv_sec - m_LastSampleTime.tv_sec) * 1000000.0 + currtime.tv_usec - m_LastSampleTime.tv_usec;

   perf->mbpsSendRate = double(m_llTraceSent) * m_iPayloadSize * 8.0 / interval;
   perf->mbpsRecvRate = double(m_llTraceRecv) * m_iPayloadSize * 8.0 / interval;

   perf->usPktSndPeriod = m_ullInterval / double(m_ullCPUFrequency);
   perf->pktFlowWindow = m_iFlowWindowSize;
   perf->pktCongestionWindow = (__int32)m_dCongestionWindow;
   perf->msRTT = m_iRTT/1000.0;
   perf->mbpsBandwidth = m_iBandwidth * m_iPayloadSize * 8.0;

   m_llTraceSent = m_llTraceRecv = m_iTraceLoss = m_iTraceRetrans = m_iSentACK = m_iRecvACK = m_iSentNAK = m_iRecvNAK = 0;

   m_LastSampleTime = currtime;
#endif   
}

void CUDT::initSynch()
{
   #ifndef WIN32
      pthread_mutex_init(&m_SendDataLock, NULL);
      pthread_cond_init(&m_SendDataCond, NULL);
      pthread_mutex_init(&m_SendBlockLock, NULL);
      pthread_cond_init(&m_SendBlockCond, NULL);
      pthread_mutex_init(&m_RecvDataLock, NULL);
      pthread_cond_init(&m_RecvDataCond, NULL);
      pthread_mutex_init(&m_OverlappedRecvLock, NULL);
      pthread_cond_init(&m_OverlappedRecvCond, NULL);
      pthread_mutex_init(&m_SendLock, NULL);
      pthread_mutex_init(&m_RecvLock, NULL);
      pthread_mutex_init(&m_AckLock, NULL);
      pthread_mutex_init(&m_ConnectionLock, NULL);
      pthread_mutex_init(&m_WindowLock, NULL);
      pthread_cond_init(&m_WindowCond, NULL);
      pthread_mutex_init(&m_HandleLock, NULL);
   #else
      m_SendDataLock = CreateMutex(NULL, false, NULL);
      m_SendDataCond = CreateEvent(NULL, false, false, NULL);
      m_SendBlockLock = CreateMutex(NULL, false, NULL);
      m_SendBlockCond = CreateEvent(NULL, false, false, NULL);
      m_RecvDataLock = CreateMutex(NULL, false, NULL);
      m_RecvDataCond = CreateEvent(NULL, false, false, NULL);
      m_OverlappedRecvLock = CreateMutex(NULL, false, NULL);
      m_OverlappedRecvCond = CreateEvent(NULL, false, false, NULL);
      m_SendLock = CreateMutex(NULL, false, NULL);
      m_RecvLock = CreateMutex(NULL, false, NULL);
      m_AckLock = CreateMutex(NULL, false, NULL);
      m_ConnectionLock = CreateMutex(NULL, false, NULL);
      m_WindowLock = CreateMutex(NULL, false, NULL);
      m_WindowCond = CreateEvent(NULL, false, false, NULL);
      m_HandleLock = CreateMutex(NULL, false, NULL);
   #endif
}

void CUDT::destroySynch()
{
   #ifndef WIN32
      pthread_mutex_destroy(&m_SendDataLock);
      pthread_cond_destroy(&m_SendDataCond);
      pthread_mutex_destroy(&m_SendBlockLock);
      pthread_cond_destroy(&m_SendBlockCond);
      pthread_mutex_destroy(&m_RecvDataLock);
      pthread_cond_destroy(&m_RecvDataCond);
      pthread_mutex_destroy(&m_OverlappedRecvLock);
      pthread_cond_destroy(&m_OverlappedRecvCond);
      pthread_mutex_destroy(&m_SendLock);
      pthread_mutex_destroy(&m_RecvLock);
      pthread_mutex_destroy(&m_AckLock);
      pthread_mutex_destroy(&m_ConnectionLock);
      pthread_mutex_destroy(&m_WindowLock);
      pthread_cond_destroy(&m_WindowCond);
      pthread_mutex_destroy(&m_HandleLock);
   #else
      CloseHandle(m_SendDataLock);
      CloseHandle(m_SendDataCond);
      CloseHandle(m_SendBlockLock);
      CloseHandle(m_SendBlockCond);
      CloseHandle(m_RecvDataLock);
      CloseHandle(m_RecvDataCond);
      CloseHandle(m_OverlappedRecvLock);
      CloseHandle(m_OverlappedRecvCond);
      CloseHandle(m_SendLock);
      CloseHandle(m_RecvLock);
      CloseHandle(m_AckLock);
      CloseHandle(m_ConnectionLock);
      CloseHandle(m_WindowLock);
      CloseHandle(m_WindowCond);
      CloseHandle(m_HandleLock);
   #endif
}

void CUDT::releaseSynch()
{
   #ifndef WIN32
      // wake up sending thread
      pthread_cond_signal(&m_WindowCond);

      pthread_mutex_lock(&m_SendDataLock);
      pthread_cond_signal(&m_SendDataCond);
      pthread_mutex_unlock(&m_SendDataLock);

      // wake up user calls
      pthread_mutex_lock(&m_SendBlockLock);
      pthread_cond_signal(&m_SendBlockCond);
      pthread_mutex_unlock(&m_SendBlockLock);

      pthread_mutex_lock(&m_SendLock);
      pthread_mutex_unlock(&m_SendLock);

      pthread_mutex_lock(&m_RecvDataLock);
      pthread_cond_signal(&m_RecvDataCond);
      pthread_mutex_unlock(&m_RecvDataLock);

      pthread_mutex_lock(&m_OverlappedRecvLock);
      pthread_cond_signal(&m_OverlappedRecvCond);
      pthread_mutex_unlock(&m_OverlappedRecvLock);

      pthread_mutex_lock(&m_RecvLock);
      pthread_mutex_unlock(&m_RecvLock);
   #else
      SetEvent(m_WindowCond);
      SetEvent(m_SendDataCond);

      SetEvent(m_SendBlockCond);
      WaitForSingleObject(m_SendLock, INFINITE);
      ReleaseMutex(m_SendLock);
      SetEvent(m_RecvDataCond);
      SetEvent(m_OverlappedRecvCond);
      WaitForSingleObject(m_RecvLock, INFINITE);
      ReleaseMutex(m_RecvLock);
   #endif
}

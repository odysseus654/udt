/********************************************************************
Copyright (c) 2001,2002, The Board of Trustees of the University of Illinois.
All Rights Reserved.

SABUL High Performance Data Transfer Protocol

Laboratory for Adavanced Computing (LAC)
National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.lac.uic.edu/

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software (SABUL) and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimers.

Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimers in the documentation
and/or other materials provided with the distribution.

Neither the names of the University of Illinois, LAC/NCDM, nor the names
of its contributors may be used to endorse or promote products derived
from this Software without specific prior written permission.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE CONTIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
********************************************************************/


/********************************************************************
sender.cpp
The sender side algorithms, rate control,  and memory management

Author: Yunhong Gu [gu@lac.uic.edu]
Last Update: Mar. 16, 2003
********************************************************************/



#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>
#include <math.h>
#include <iostream>
#include "sender.h"


//
CSabulSender::CSabulSender():
m_iEXPInterval(1000000),
m_dLossRateLimit(0.001),
m_dWeight(0.1)
{
   m_pBuffer = NULL;
   m_pLossList = NULL;

   pthread_mutex_init(&m_SendDataLock, NULL);
   pthread_cond_init(&m_SendDataCond, NULL);

   pthread_mutex_init(&m_SendBlockLock, NULL);
   pthread_cond_init(&m_SendBlockCond, NULL);

   m_dInterval = 10.0;
   m_dLowerLimit = 1000.0;
   m_dUpperLimit = 10.0;
}

CSabulSender::~CSabulSender()
{
   if (m_pBuffer)
      delete m_pBuffer;
   if (m_pLossList)
      delete m_pLossList;
}

//
// "open" method initilize the sender and bind it to the "port" number.
// Returned value is the real port number it is bound to.
//

int CSabulSender::open(const int& port)
{
   m_bClosing = false;
   m_lLastAck = 0;
   m_ullInterval = (unsigned long long)(m_dInterval * m_ullCPUFrequency);
   m_lLocalSend = 0;
   m_lLastSYNSeqNo = 0;
   m_lCurrSeqNo = 0;
   m_dHistoryRate = 0.0;
   m_iERRCount = 0;
   m_iDecCount = 1;
   m_lLastDecSeq = -1;
   m_bFreeze = false;

   if (-1 != port)
      m_iDataPort = m_iCtrlPort = port;

   try
   {
      m_pDataChannel = new CUdpChannel(m_iDataPort);
      m_pCtrlChannel = new CTcpChannel(m_iCtrlPort);
   }
   catch(...)
   {
      if (0 != m_iVerbose)
         m_Log << "Channel Initlization Failed." << endl;
      else
         cout << "Channel Initlization Failed." << endl;
      throw;
   }

   if (0 != m_iVerbose)
      m_Log << "Data Channel is ready at port: " << m_iDataPort << endl;
   else
      cout<< "Data Channel is ready at port: " << m_iDataPort << endl;
   if (0 != m_iVerbose)
      m_Log << "Control Channel is ready at port: " << m_iCtrlPort << endl;
   else
      cout<< "Control Channel is ready at port: " << m_iCtrlPort << endl;

   return m_iCtrlPort;
}

//
// Listening to a receiver to connect to and exchange information.
//

void CSabulSender::listen()
{
   try
   {
      sockaddr_in remoteaddr = m_pCtrlChannel->connect();
      char* ip = inet_ntoa(remoteaddr.sin_addr);
      m_pCtrlChannel->recv((char *)&m_RemoteOpt, sizeof(SabulOption));
      m_pCtrlChannel->send((char *)&m_RemoteOpt, sizeof(SabulOption));
      m_pDataChannel->connect(ip, m_RemoteOpt.m_iDataPort);
   }
   catch(...)
   {
      if (0 != m_iVerbose)
         m_Log << "Channel Connection Failed." << endl;
      else
         cout << "Channel Connection Failed." << endl;
      throw;
   }

   m_iSendFlagSize = m_RemoteOpt.m_iRecvFlagSize;
   m_iWarningLine = int(m_iSendFlagSize * 0.75);

   m_pBuffer = new CBuffer(m_lSendBufSize);
   m_pLossList = new CLossList;

   m_bConnection = true;

   gettimeofday(&m_LastSYNTime, 0);

   pthread_create(m_pDCThread, NULL, CSabulSender::dcHandler, this);
}

//
// Close the connection.
//

void CSabulSender::close()
{
   m_bClosing = true;

   pthread_cond_signal(&m_SendDataCond);

   pthread_join(*m_pDCThread, NULL);

   m_pDataChannel->disconnect();
   m_pCtrlChannel->disconnect();
   delete m_pDataChannel;
   delete m_pCtrlChannel;

   delete m_pBuffer;
   delete m_pLossList;
   m_pBuffer = NULL;
   m_pLossList = NULL;
}

//
// The main SABUL sending algorithm can be found in this private method.
// dcHandler() is a used as a thread handle keeping on sending data and receiving feedback.
//

void* CSabulSender::dcHandler(void* sender)
{
   CSabulSender* self = static_cast<CSabulSender *>(sender);

   CPacketVector sdpkt(self->m_iPktSize);
   int payload;
   long offset;

   CPktSblCtrl scpkt(self->m_iPktSize);

   unsigned long long int entertime;
   unsigned long long int nextexptime;
   self->rdtsc(nextexptime);

   unsigned long long int ullexpint = self->m_iEXPInterval * self->m_ullCPUFrequency;

   nextexptime += ullexpint;

   while (!self->m_bClosing)
   {
      self->rdtsc(entertime);

      //
      // if the sender hasn't receive any feedback from the receiver, 
      // the EXP timer expired and generate an EXP event.
      //

      if (entertime > nextexptime)
      {
         self->processFeedback(SC_EXP);
         nextexptime = entertime + ullexpint;
      }

      //
      // if there is no data to send, wait here...
      //

      if (0 == self->m_pBuffer->getCurrBufSize())
      {
         pthread_mutex_lock(&(self->m_SendDataLock));
         while ((0 == self->m_pBuffer->getCurrBufSize()) && (!self->m_bClosing))
            pthread_cond_wait(&(self->m_SendDataCond), &(self->m_SendDataLock));
         pthread_mutex_unlock(&(self->m_SendDataLock));

         self->rdtsc(nextexptime);
         nextexptime += ullexpint;
      }

      //
      // Poll the control channel to see if there is any feedback...
      //

      *(CTcpChannel *)(self->m_pCtrlChannel) >> scpkt;
      if (scpkt.getLength() > 0)
      {
         //
         // Process them if there is any...
         //
         self->processFeedback(PktType(scpkt.m_lPktType), scpkt.m_lAttr, scpkt.m_plData);
         //
         // Reset the EXP timer...
         //
         if (scpkt.m_lPktType != SC_SYN)
         {
            self->rdtsc(nextexptime);
            nextexptime += ullexpint;
         }
      }
      else if (scpkt.getLength() < 0)
      {
         //
         // Something wrong with the connection...broken!
         //
         self->m_bConnection = false;
         break;
      }

      //
      // Now sending, check the loss list first...
      //
      if ((sdpkt.m_lSeqNo = self->m_pLossList->getLostSeq()) >= 0)
      {
         if ((sdpkt.m_lSeqNo >= self->m_lLastAck) && (sdpkt.m_lSeqNo < self->m_lLastAck + self->m_lSeqNoTH))
            offset = (sdpkt.m_lSeqNo - self->m_lLastAck) * self->m_iPayloadSize;
         else if (sdpkt.m_lSeqNo < self->m_lLastAck - self->m_lSeqNoTH)
            offset = (sdpkt.m_lSeqNo + self->m_lMaxSeqNo - self->m_lLastAck) * self->m_iPayloadSize;
         else
            continue;

         if ((payload = self->m_pBuffer->readData(&(sdpkt.m_pcData), offset, self->m_iPayloadSize)) == 0)
            continue;

         self->m_lLocalSend ++;
      }
      //
      // If there is no loss, send a new packet.
      //
      else
      {
         if (self->m_lCurrSeqNo - self->m_lLastAck >= self->m_iSendFlagSize)
            continue;

         if ((payload = self->m_pBuffer->readData(&(sdpkt.m_pcData), self->m_iPayloadSize)) == 0)
            continue;

         sdpkt.m_lSeqNo = self->m_lCurrSeqNo ++;
         self->m_lCurrSeqNo %= self->m_lMaxSeqNo;
      }
 
      sdpkt.setLength(payload);

      //
      // Convert to network order...
      //
      sdpkt.m_lSeqNo = htonl(sdpkt.m_lSeqNo);

      *(CUdpChannel *)(self->m_pDataChannel) << sdpkt;

      //
      // Waiting for the inter-packet time.
      //
      if (self->m_bFreeze)
      {
         self->sleepto(entertime + self->m_iSYNInterval * self->m_ullCPUFrequency + self->m_ullInterval);
         self->m_bFreeze = false;
      }
      else
         self->sleepto(entertime + self->m_ullInterval);
   }

   return NULL;
}

void CSabulSender::processFeedback(const PktType type, const long attr, const long* data)
{
   switch (type)
   {
   case SC_ACK: // Positive acknowledgement
      if (attr >= m_lLastAck)
         m_pBuffer->ackData((attr - m_lLastAck) * m_iPayloadSize, m_iPayloadSize);
      else
         m_pBuffer->ackData((attr - m_lLastAck + m_lMaxSeqNo) * m_iPayloadSize, m_iPayloadSize);
      m_lLastAck = attr;
      m_pLossList->remove(m_lLastAck);

      pthread_mutex_lock(&m_SendBlockLock);
      if ((m_bBlockable) && (0 == m_pBuffer->getCurrBufSize()))
         pthread_cond_signal(&m_SendBlockCond);
      pthread_mutex_unlock(&m_SendBlockLock);

      break;

   case SC_SYN: // Calcualte loss rate and do rate control here
      m_lLocalSend += m_lCurrSeqNo - m_lLastSYNSeqNo;
      if (m_lCurrSeqNo < m_lLastSYNSeqNo)
         m_lLocalSend += m_lMaxSeqNo;

      if (m_lLocalSend < m_lLocalERR)
         m_lLocalSend = m_lLocalERR;

      if (m_lLocalSend != 0)
      {
         if (0 != m_iVerbose)
            m_Log << "loss rate: "<< double(m_lLocalERR) / double(m_lLocalSend) * 100 << "%" << "\t\t";
         else
         {
            cout<< "loss rate: ";
            cout.width(10);
            cout<< double(m_lLocalERR) / double(m_lLocalSend) * 100 << "%" << "\t\t";
         }
 
         rateControl(double(m_lLocalERR) / double(m_lLocalSend));

         timeval currtime;
         gettimeofday(&currtime, 0);

         if (0 != m_iVerbose)
            m_Log << "sending rate: "<< double(m_lLocalSend) * 1500.0 * 8.0 / ((currtime.tv_sec - m_LastSYNTime.tv_sec) * 1000000 + (currtime.tv_usec - m_LastSYNTime.tv_usec)) << "Mbps" << endl;
         else
            cout<< "sending rate: "<< double(m_lLocalSend) * 1500.0 * 8.0 / ((currtime.tv_sec - m_LastSYNTime.tv_sec) * 1000000 + (currtime.tv_usec - m_LastSYNTime.tv_usec)) << "Mbps" << endl;

         m_LastSYNTime = currtime;

         m_ullInterval = (unsigned long long)(m_dInterval * m_ullCPUFrequency);
      }

      m_lLastSYNSeqNo = m_lCurrSeqNo;
      m_lLocalSend = 0;
      m_lLocalERR = 0;

      break;

   case SC_ERR: // Loss, insert the loss seq. no. into the loss list.
/*
      if (10 == ++ m_iERRCount)
      {
         m_dInterval += 2.0;
         m_ullInterval += 2 * m_ullCPUFrequency;
         m_iERRCount = -500;
      }
*/

      // Rate Control on Loss
      if (((data[0] > m_lLastDecSeq) && (data[0] - m_lLastDecSeq < m_lSeqNoTH)) || (data[0] < m_lLastDecSeq - m_lSeqNoTH))
      {
         m_dInterval = m_dInterval * 1.125;

         m_lLastDecSeq = m_lCurrSeqNo;

         m_bFreeze = true;

         m_iERRCount = 1;
         m_iDecCount = 4;
      }
      else if (++ m_iERRCount >= pow(2.0, m_iDecCount))
      {
         m_iDecCount ++;

         m_dInterval = m_dInterval * 1.125;
      }

      m_ullInterval = (unsigned long long)(m_dInterval * m_ullCPUFrequency);

      for (int i = attr - 1; (i >= 0) && (m_pLossList->insert(data[i])); i --) {}

      m_lLocalERR += attr;

      break;

   case SC_EXP:
      for (int i = m_lCurrSeqNo - m_lLastAck - 1; (i >= 0) && (m_pLossList->insert(i + m_lLastAck)); i --) {}
 
      break;

   default:
      break;
   }
}

//
// Part of the rate control, the other part is in the processFeedback() SC_ERR section.
//

void CSabulSender::rateControl(const double& currlossrate)
{
   m_dHistoryRate = m_dHistoryRate * m_dWeight + currlossrate * (1 - m_dWeight);

/*
   if (m_dHistoryRate > m_dLossRateLimit)
      m_dInterval = m_dInterval * (1. + 0.1 * (m_dHistoryRate - m_dLossRateLimit)) + 0.5;
   else if (m_dHistoryRate < m_dLossRateLimit)
      m_dInterval = m_dInterval * (1. + 10.0 * (m_dHistoryRate - m_dLossRateLimit));
   else
      m_dInterval += 0.1;
*/

   if (m_dHistoryRate > m_dLossRateLimit)
      return;

   double inc = pow(10, ceil(log10(m_iSYNInterval / m_dInterval))) / 1000.0;

   if (inc < 1.0/1500)
      inc = 1.0/1500;

   m_dInterval = (m_dInterval * m_iSYNInterval) / (m_dInterval * inc + m_iSYNInterval);

   if (m_dInterval > m_dLowerLimit)
         m_dInterval = m_dLowerLimit;
   else if (m_dInterval < m_dUpperLimit)
         m_dInterval = m_dUpperLimit;
}

//
// API send
// 

bool CSabulSender::send(const char* data, const long& len)
{
   if (!m_bConnection)
      throw int(-1);

   if (len > m_pBuffer->getSizeLimit())
      throw int(-2);

   bool ret;

   pthread_mutex_lock(&m_SendDataLock);
   ret = m_pBuffer->addBuffer(data, len, m_bBlockable);
   pthread_mutex_unlock(&m_SendDataLock);

   if (ret)
      pthread_cond_signal(&m_SendDataCond);

   pthread_mutex_lock(&m_SendBlockLock);
   while ((m_bBlockable) && (0 != m_pBuffer->getCurrBufSize()))
      pthread_cond_wait(&m_SendBlockCond, &m_SendBlockLock);
   pthread_mutex_unlock(&m_SendBlockLock);

   return ret;
}

//
// API sendfile
//

bool CSabulSender::sendfile(const int& fd, const int& offset, const int& size)
{
   if (!m_bConnection)
      throw int(-1);

   bool block = m_bBlockable;
   m_bBlockable = false;

   char* tempbuf;
   long unitsize = 7340000;
   int count = 1;

   while (unitsize * count <= size)
   {
      tempbuf = new char[unitsize];
      read(fd, tempbuf, unitsize);

      pthread_mutex_lock(&m_SendDataLock);
      while (!m_pBuffer->addBuffer(tempbuf, unitsize))
         usleep(10);
      pthread_mutex_unlock(&m_SendDataLock);

      pthread_cond_signal(&m_SendDataCond);

      count ++;
   }
   if (size - unitsize * (count - 1) > 0)
   {
      tempbuf = new char[size - unitsize * (count - 1)];
      read(fd, tempbuf, size - unitsize * (count - 1));

      pthread_mutex_lock(&m_SendDataLock);
      while (!m_pBuffer->addBuffer(tempbuf, size - unitsize * (count - 1)))
         usleep(10);
      pthread_mutex_unlock(&m_SendDataLock);

      pthread_cond_signal(&m_SendDataCond);

      count ++;
   }

   while (getCurrBufSize())
      usleep(10);

//
// The following mmap solution has unknown mistakes...
// So it is NOT used any more.
//

/*
   size_t pagesize = getpagesize(); //4096
   long mapsize = pagesize * (m_iPayloadSize >> 2);
   char* filemap;
   int count = 1;

   while (mapsize * count <= size)
   {
      filemap = (char *)mmap(0, mapsize, PROT_READ, MAP_SHARED, fd, (count - 1) * mapsize);

      pthread_mutex_lock(&m_SendDataLock);
      while (!m_pBuffer->addBuffer(filemap, mapsize, true))
         usleep(10);
      pthread_mutex_unlock(&m_SendDataLock);

      pthread_cond_signal(&m_SendDataCond);

      count ++;
   }
   if (size - mapsize * (count - 1) > 0)
   {
      filemap = (char *)mmap(0, size - mapsize * (count - 1), PROT_READ, MAP_SHARED, fd, (count - 1) * mapsize);

      pthread_mutex_lock(&m_SendDataLock);
      while (!m_pBuffer->addBuffer(filemap, size - mapsize * (count - 1), true))
         usleep(10);
      pthread_mutex_unlock(&m_SendDataLock);

      pthread_cond_signal(&m_SendDataCond);
   }
*/

   m_bBlockable = block;

   return true;
}

long CSabulSender::getCurrBufSize() const
{
   if (!m_bConnection)
      throw int(-1);

   return m_pBuffer->getCurrBufSize();
}

void CSabulSender::setRate(const double& initrate, const double& l_limit = 0, const double& u_limit = 1000) //in Mbps
{
   m_dInterval = (1500.0 * 8.0) / initrate;

   if (l_limit > 0)
      m_dLowerLimit = (1500.0 * 8.0) / l_limit;
   else
      m_dLowerLimit = 1000.0;

   m_dUpperLimit = (1500.0 * 8.0) / u_limit;
}

//
// The buffer management codes, see reference in sender.h
//

//
CSabulSender::CBuffer::CBuffer():
m_pBlock(NULL),
m_pLastBlock(NULL),
m_pCurrSendBlk(NULL),
m_pCurrAckBlk(NULL),
m_lCurrBufSize(0),
m_lSizeLimit(40960000)
{
   pthread_mutex_init(&m_BufLock, NULL);
}

CSabulSender::CBuffer::CBuffer(const long& bufsize)
{
   CBuffer::CBuffer();

   m_lSizeLimit = bufsize;
}

CSabulSender::CBuffer::~CBuffer()
{
   Block* pb = m_pBlock;
   while (NULL != m_pBlock)
   {
      pb = pb->m_next;
      if (m_pBlock->m_bMap)
         munmap(m_pBlock->m_pcData, m_pBlock->m_lLength);
      else
         delete [] m_pBlock->m_pcData;
      delete m_pBlock;
      m_pBlock = pb;
   }
}

bool CSabulSender::CBuffer::addBuffer(const char* data, const long& len, const bool map)
{
   CGuard bufferguard(m_BufLock);

   if (m_lCurrBufSize + len > m_lSizeLimit)
      return false;

   if (NULL == m_pBlock)
   {
      m_pBlock = new Block;
      m_pBlock->m_pcData = const_cast<char *>(data);
      m_pBlock->m_lLength = len;
      m_pBlock->m_bMap = map;
      m_pBlock->m_next = NULL;
      m_pLastBlock = m_pBlock;
      m_pCurrSendBlk = m_pBlock;
      m_lCurrSendPnt = 0;
      m_pCurrAckBlk = m_pBlock;
      m_lCurrAckPnt = 0;
   }
   else
   {
      m_pLastBlock->m_next = new Block;
      m_pLastBlock = m_pLastBlock->m_next;
      m_pLastBlock->m_pcData = const_cast<char *>(data);
      m_pLastBlock->m_lLength = len;
      m_pLastBlock->m_bMap = map;
      m_pLastBlock->m_next = NULL;
      if (NULL == m_pCurrSendBlk)
         m_pCurrSendBlk = m_pLastBlock;
   }

   m_lCurrBufSize += len;
   return true;
}

long CSabulSender::CBuffer::readData(char** data, const long& len)
{
   if (NULL == m_pCurrSendBlk)
      return 0;

   if (m_lCurrSendPnt + len < m_pCurrSendBlk->m_lLength)
   {
      *data = m_pCurrSendBlk->m_pcData + m_lCurrSendPnt;
      m_lCurrSendPnt += len;
      return len;
   }

   long readlen = m_pCurrSendBlk->m_lLength - m_lCurrSendPnt;
   *data = m_pCurrSendBlk->m_pcData + m_lCurrSendPnt;
 
   m_pCurrSendBlk = m_pCurrSendBlk->m_next;
   m_lCurrSendPnt = 0;     
    
   return readlen;
}

long CSabulSender::CBuffer::readData(char** data, const long offset, const long& len)
{
   CGuard bufferguard(m_BufLock);

   Block* p = m_pCurrAckBlk;
   if (NULL == p)
      return 0;
   long loffset = offset + m_lCurrAckPnt;
   while (p->m_lLength <= loffset)
   {
      loffset -= p->m_lLength;
      loffset -= len - ((0 == p->m_lLength % len) ? len : (p->m_lLength % len));
      p = p->m_next;
      if (NULL == p)
         return 0;
   }

   if (loffset + len <= p->m_lLength)
   {
      *data = p->m_pcData + loffset;
      return len;
   }

   *data = p->m_pcData + loffset;
   return p->m_lLength - loffset;
}

void CSabulSender::CBuffer::ackData(const long& len, const int& payloadsize)
{
   CGuard bufferguard(m_BufLock);

   m_lCurrAckPnt += len;
   while (m_lCurrAckPnt >= m_pCurrAckBlk->m_lLength)
   {
      m_lCurrAckPnt -= m_pCurrAckBlk->m_lLength;
      if (0 != m_pCurrAckBlk->m_lLength % payloadsize)
         m_lCurrAckPnt -= payloadsize - m_pCurrAckBlk->m_lLength % payloadsize;

      m_lCurrBufSize -= m_pCurrAckBlk->m_lLength;
      m_pCurrAckBlk = m_pCurrAckBlk->m_next;

      if (m_pBlock->m_bMap)
         munmap(m_pBlock->m_pcData, m_pBlock->m_lLength);
      else
         delete [] m_pBlock->m_pcData;
      delete m_pBlock;
      m_pBlock = m_pCurrAckBlk;

      if (NULL == m_pBlock)
         break;
   }
}

long CSabulSender::CBuffer::getCurrBufSize() const
{
   return m_lCurrBufSize;
}

long CSabulSender::CBuffer::getSizeLimit() const
{
   return m_lSizeLimit;
}

//
// The loss list management codes, see reference in sender.h
//

//
CSabulSender::CLossList::CLossList():
m_lAttr(0),
m_next(NULL)
{
}

CSabulSender::CLossList::CLossList(const long& seqno):
m_lAttr(seqno),
m_next(NULL)
{
}

int CSabulSender::CLossList::insert(const long& seqno)
{
   CLossList *p = this;
   while ((NULL != p->m_next) && (((p->m_next->m_lAttr < seqno) && (seqno - p->m_next->m_lAttr < (1 << 20))) || (p->m_next->m_lAttr - seqno > (1 << 20))))
      p = p->m_next;

   if (NULL == p->m_next)
   {
      p->m_next = new CLossList(seqno);
      m_lAttr ++;
      return 1;
   }

   if (seqno == p->m_next->m_lAttr)
      return 0;

   CLossList *q = new CLossList(seqno);
   q->m_next = p->m_next;
   p->m_next = q;
   m_lAttr ++;
   return 1;
}

void CSabulSender::CLossList::remove(const long& seqno)
{
   CLossList *p = this->m_next;
   while ((NULL != p) && (((p->m_lAttr <= seqno) && (seqno - p->m_lAttr < (1 << 20))) || (p->m_lAttr - seqno > (1 << 20))))
   {
      m_next = p->m_next;
      delete p;
      p = m_next;

      m_lAttr --;
   }
}

int CSabulSender::CLossList::getLossLength() const
{
   return m_lAttr;
}

long CSabulSender::CLossList::getLostSeq()
{
   if (NULL == m_next)
      return -1;

   long seqno = m_next->m_lAttr;
   CLossList *p = m_next;
   m_next = p->m_next;
   delete p;

   m_lAttr --;

   return seqno;
}

//

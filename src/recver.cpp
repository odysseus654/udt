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
recver.cpp
The SABUL receiver algorithms and memory management

Author: Yunhong Gu [gu@lac.uic.edu]
Last Update: Jan. 07, 2003
********************************************************************/


#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>
#include "recver.h"

//
// These intervals are fixed in SABUL and MUST NOT be changed.
// 

//
CSabulRecver::CSabulRecver():
m_iACKInterval(100000),
m_iERRInterval(20000)
{
   m_pBuffer = NULL;
   m_pIrregularPktList = NULL;
   m_pLossList = NULL;

   pthread_cond_init(&m_RecvDataCond, NULL);
   pthread_mutex_init(&m_RecvDataLock, NULL);
}

CSabulRecver::~CSabulRecver()
{
   if (m_pBuffer)
      delete m_pBuffer;
   if (m_pIrregularPktList)
      delete m_pIrregularPktList;
   if (m_pLossList)
      delete m_pLossList;
}

//
// Bind the receiver to a local (any available) port and
// connect to the sender at "ip":"port"
//

int CSabulRecver::open(const char* ip, const int& port)
{
   m_bClosing = false;
   m_RecvFlag.reset();
   m_lLastAck = 0;
   m_lLocalRecv = 0;
   m_lNextExpect = 0;
   m_lCurrSeqNo = -1;
   m_bReadBuf = false;
   
   try
   {
      m_pDataChannel = new CUdpChannel(m_iDataPort);
      m_pCtrlChannel = new CTcpChannel(m_iCtrlPort);
   }
   catch(...)
   {
      //cout<< "Channel Initilization Failed." << endl;
      throw;
   }

   //cout<< "Data Channel is ready at port: " << m_iDataPort << endl;
   //cout<< "Control Channel is ready at port: " << m_iCtrlPort << endl;

   timeval time1, time2;

   try
   {
      m_pCtrlChannel->connect(ip, port);
      SabulOption opt;
      opt.m_iDataPort = m_iDataPort;
      opt.m_iRecvFlagSize = FlagSize;

      m_pCtrlChannel->send((char *)&opt, sizeof(SabulOption));
      gettimeofday(&time1, 0);
      m_pCtrlChannel->recv((char *)&opt, sizeof(SabulOption));
      gettimeofday(&time2, 0);
      m_pDataChannel->connect();

      //Calculate RTT here. RTT is modified so that it is not too small.
      // This is NOT REAL RTT. We will have more accurate method to calcualte in future version
      m_iRTT = (time2.tv_sec - time1.tv_sec) * 1000000 + time2.tv_usec - time1.tv_usec;
      m_iRTT = (m_iRTT > m_iERRInterval) ? int(m_iRTT * 1.5) : m_iERRInterval;
   }
   catch(...)
   {
      //cout<< "Channel Connection Failed." << endl;
      throw;
   }

   m_pBuffer = new CBuffer(m_lRecvBufSize);
   m_pIrregularPktList = new CIrregularPktList;
   m_pLossList = new CLossList;

   m_bConnection = true;
   
   pthread_create(m_pDCThread, NULL, CSabulRecver::dcHandler, this);

   return m_iCtrlPort;
}

void CSabulRecver::close()
{
   m_bClosing = true;
   m_bConnection = false;

   pthread_join(*m_pDCThread, NULL);

   m_pDataChannel->disconnect();
   m_pDataChannel->disconnect();
   delete m_pDataChannel;
   delete m_pCtrlChannel;

   delete m_pBuffer;
   delete m_pIrregularPktList;
   delete m_pLossList;
   m_pBuffer = NULL;
   m_pIrregularPktList = NULL;
   m_pLossList = NULL;   
}

//
// The receiving thread handle.
// Main receiving algorithm is here.
//
void* CSabulRecver::dcHandler(void* recver)
{
   CSabulRecver* self = static_cast<CSabulRecver *>(recver);
   CPacketVector sdpkt(self->m_iPayloadSize);
   char payload[self->m_iPayloadSize];
   bool nextslotfound;
   long offset;
   long lossdata[self->m_iMaxLossLength];

   //
   // Uses CPU clock cycles to obtain high precision timing
   //
   unsigned long long int currtime;
   unsigned long long int nextacktime;
   unsigned long long int nexterrtime;
   unsigned long long int nextsyntime;
   unsigned long long int ullackint = self->m_iACKInterval * self->m_ullCPUFrequency;
   unsigned long long int ullerrint = self->m_iERRInterval * self->m_ullCPUFrequency;
   unsigned long long int ullsynint = self->m_iSYNInterval * self->m_ullCPUFrequency;

   self->rdtsc(nextacktime);
   nextacktime += ullackint;
   self->rdtsc(nextsyntime);
   nextsyntime += ullsynint;

   while (!self->m_bClosing)
   {
      //
      // If the "recv" is called and there is not enough data to fill the buffer in the request size,
      // register the user buffer here.
      //
      if (self->m_bReadBuf)
      {
         pthread_mutex_lock(&(self->m_RecvDataLock));
         self->m_bReadBuf = self->m_pBuffer->readBuffer(const_cast<char*>(self->m_pcTempData), const_cast<long&>(self->m_lTempLen));
         pthread_mutex_unlock(&(self->m_RecvDataLock));

         if (!self->m_bReadBuf)
            self->m_pBuffer->registerUserBuf(const_cast<char*>(self->m_pcTempData), const_cast<long&>(self->m_lTempLen));
         else
         {
            self->m_bReadBuf = false;
            pthread_cond_signal(&(self->m_RecvDataCond));
         }
      }

      //
      // Check if the ACK timer is expired
      //
      self->rdtsc(currtime);
      if ((currtime > nextacktime) || self->m_pBuffer->reachUserBufBoundary())
      {
         self->feedback(SC_ACK);
         nextacktime = currtime + ullackint;
      }
      //
      // Check if the ERR timer is expired
      //
      if ((currtime > nexterrtime) && (self->m_pLossList->getLossLength() > 0))
      {
         self->feedback(SC_ERR);
         nexterrtime = currtime + ullerrint;
      }
      //
      // Check if the SYN timer is expired
      //
      if (currtime > nextsyntime)
      {
         self->feedback(SC_SYN);
         nextsyntime = currtime + ullsynint;
      }

      //
      // It is expected to received a packet in the size of fixed "m_iPayloadSize"
      //
      sdpkt.setLength(self->m_iPayloadSize);

      //
      // Speculate the next slot in the protocol buffer and return the position of the memory to put the received data
      //
      if (!(self->m_pBuffer->nextDataPos(&(sdpkt.m_pcData), self->m_lNextExpect * self->m_iPayloadSize - self->m_pIrregularPktList->currErrorSize(self->m_lNextExpect + self->m_lLastAck), self->m_iPayloadSize)))
      {
         //
         // If failed, use temporay buffer
         //
         sdpkt.m_pcData = payload;
         nextslotfound = false;
      }
      else
         nextslotfound = true;

      //
      // Timed received. The receive will wait for 100 microseconds before it finally receives a packets or the timer expired
      //
      *(CUdpChannel *)(self->m_pDataChannel) >> sdpkt;

      //
      // Got nothing...continue to next round.
      //
      if (sdpkt.getLength() <= 0)
         continue;

      self->m_lLocalRecv ++;

      //
      // Convert to the local bit order
      //
      sdpkt.m_lSeqNo = ntohl(sdpkt.m_lSeqNo);

      //
      // Calculate the offset in the m_RecvFlag.
      //
      offset = sdpkt.m_lSeqNo - self->m_lLastAck;

      if (offset >= FlagSize)
      {
         self->feedback(SC_ACK);
         self->rdtsc(nextacktime);
         nextacktime += ullackint;
         continue;
      }
      else if (offset < 0)
         if (offset < -self->m_lSeqNoTH)
            offset += self->m_lMaxSeqNo;
         else
            continue;

      //
      // If the speculcation is wrong...
      //
      if ((offset != self->m_lNextExpect) || (!nextslotfound))
      {
         //
         // Put the data explicitly into the right position.
         //
         if (!(self->m_pBuffer->addData(sdpkt.m_pcData, offset * self->m_iPayloadSize - self->m_pIrregularPktList->currErrorSize(sdpkt.m_lSeqNo), sdpkt.getLength() - sizeof(long))))
            continue;

         //
         // Loss detected...
         //
         if (offset > self->m_lNextExpect)
         {
            int losslen = 0;
            int start = (self->m_lCurrSeqNo + 1 >= self->m_lLastAck) ? (self->m_lCurrSeqNo - self->m_lLastAck + 1) : (self->m_lCurrSeqNo + self->m_lMaxSeqNo - self->m_lLastAck + 1);
            for (int i = start; i < offset; i ++)
               if (!self->m_RecvFlag[i])
               {
                  self->m_pLossList->insert((i + self->m_lLastAck) % self->m_lMaxSeqNo);
                  if (losslen < self->m_iMaxLossLength)
                     lossdata[losslen ++] = (i + self->m_lLastAck) % self->m_lMaxSeqNo;
               }
            //
            // Report loss as soon as possible.
            //
            self->feedback(SC_ERR, losslen, lossdata);
         }
      }

      //
      // Not a regular fixed size packet...
      //
      if (sdpkt.getLength() != self->m_iPktSize)
         self->m_pIrregularPktList->addIrregularPkt(sdpkt.m_lSeqNo, self->m_iPktSize - sdpkt.getLength());

      //
      // Update the current largest received seq. no.
      // 
      if ((sdpkt.m_lSeqNo > self->m_lCurrSeqNo) && (sdpkt.m_lSeqNo - self->m_lCurrSeqNo < self->m_lSeqNoTH))
         self->m_lCurrSeqNo = sdpkt.m_lSeqNo;
      else if (sdpkt.m_lSeqNo < self->m_lCurrSeqNo - self->m_lSeqNoTH)
         self->m_lCurrSeqNo = sdpkt.m_lSeqNo;
      else 
      {
         //
         // Remove the seq. no. from the loss list
         //
         self->m_pLossList->remove(sdpkt.m_lSeqNo);
         
         if (sdpkt.getLength() < self->m_iPktSize)
            self->m_pBuffer->moveData((offset + 1) * self->m_iPayloadSize - self->m_pIrregularPktList->currErrorSize(sdpkt.m_lSeqNo), self->m_iPktSize - sdpkt.getLength());
      }

      //
      // Speculate next packet
      //
      self->getNextExpect(offset);

      self->m_RecvFlag.set(offset);
   }

   return NULL;
}

//
// Generates feedback.
//

void CSabulRecver::feedback(PktType type, const int& len, const long* data)
{
   CPktSblCtrl scpkt(m_iPktSize);

   switch (type)
   {
   case SC_ACK: // Acknowledgement
      //
      // If there is no loss, the ACK seq. no. is the current largest received seq. no plus 1.
      //
      if (0 == m_pLossList->getLossLength())
      {
         if (m_lCurrSeqNo >= m_lLastAck)
            scpkt.m_lAttr = m_lCurrSeqNo - m_lLastAck + 1;
         else if (m_lLastAck - m_lCurrSeqNo > m_lSeqNoTH)
            scpkt.m_lAttr = m_lCurrSeqNo + m_lMaxSeqNo - m_lLastAck + 1;
      }
      //
      // Otherwise it is the smallest lost seq. no.
      //
      else
         scpkt.m_lAttr = (m_pLossList->getFirstLostSeq() >= m_lLastAck) ? m_pLossList->getFirstLostSeq() - m_lLastAck : m_pLossList->getFirstLostSeq() + m_lMaxSeqNo - m_lLastAck;

      //
      // If there is new packets to acknowledge...
      //
      if (0 < scpkt.m_lAttr)
      {
         m_RecvFlag >>= scpkt.m_lAttr;
         m_lNextExpect -= scpkt.m_lAttr;
         m_lLastAck = (m_lLastAck + scpkt.m_lAttr) % m_lMaxSeqNo;

         if (m_pBuffer->ackData(scpkt.m_lAttr * m_iPayloadSize - m_pIrregularPktList->currErrorSize(m_lLastAck)))
            pthread_cond_signal(&m_RecvDataCond);

         m_pIrregularPktList->deleteIrregularPkt(m_lLastAck);

         scpkt.m_lPktType = SC_ACK;
         scpkt.m_lAttr = m_lLastAck;
         scpkt.setLength(2 * sizeof(long));
         *(CTcpChannel *)(m_pCtrlChannel) << scpkt;
      }

      break;

   case SC_ERR: //Loss Report
      if (len > 0)
      {
         scpkt.m_lAttr = len;
         memcpy(scpkt.m_plData, data, len * sizeof(long));
      }
      else if (NULL == data)
      {
         m_pLossList->getLossArray((long *)(scpkt.m_plData), int(scpkt.m_lAttr), m_iMaxLossLength, m_iRTT);

         if (0 == scpkt.m_lAttr)
            break;
      }
      else
         break;

      scpkt.m_lPktType = SC_ERR;
      scpkt.setLength((scpkt.m_lAttr + 2) * sizeof(long));
      *(CTcpChannel *)(m_pCtrlChannel) << scpkt;

      break;

   case SC_SYN: // SYN, the ATTR field is useless in the current version and will be obsolete in the next version
      scpkt.m_lPktType = SC_SYN;
      scpkt.m_lAttr = m_lLocalRecv;
      scpkt.setLength(2 * sizeof(long));

      m_lLocalRecv = 0;

      *(CTcpChannel *)(m_pCtrlChannel) << scpkt;

      break;

   default:
 
     break;
   }
}

//
// The next expected packet is the first unreceived packet since the last received packet
//

void CSabulRecver::getNextExpect(const long& offset)
{
   m_lNextExpect = (offset + 1) % FlagSize;

   while (m_RecvFlag[m_lNextExpect])
   {
      m_lNextExpect ++;
      m_lNextExpect %= FlagSize;
      if (m_lNextExpect == offset)
      {
         feedback(SC_ACK);
         m_lNextExpect = FlagSize;
      }
   }
}

//
// API recv
//

long CSabulRecver::recv(char* data, const long& len)
{
   if (len > m_pBuffer->getSizeLimit())
      throw int(-2);

   if (m_pBuffer->readBuffer(data, len))
      return len;

   pthread_mutex_lock(&m_RecvDataLock);

   m_pcTempData = data;
   m_lTempLen = len;
   m_bReadBuf = true;

   pthread_cond_wait(&m_RecvDataCond, &m_RecvDataLock);
   pthread_mutex_unlock(&m_RecvDataLock);

   return len;
}

//
// API recvfile
//

long CSabulRecver::recvfile(const int& fd, const int& offset, const int& size)
{
   long unitsize = 367000;
   int count = 1;
   char tempbuf[unitsize];

   lseek(fd, 0, SEEK_SET);

   while (unitsize * count <= size)
   {
      recv(tempbuf, unitsize);
      write(fd, tempbuf, unitsize);
      count ++;
   }
   if (size - unitsize * (count - 1) > 0)
   {
      recv(tempbuf, size - unitsize * (count - 1));
      write(fd, tempbuf, size - unitsize * (count - 1));
   }

//
// The following codes is an obsolete version with mmap.
//

/*
   size_t pagesize = getpagesize(); //4096
   long mapsize = pagesize * (m_iPayloadSize >> 2);
   char* filemap;
   int count = 1;

   lseek(fd, size - 1, SEEK_SET);
   write(fd, " ", 1);

   while (mapsize * count <= size)
   {
      filemap = (char *)mmap(0, mapsize, PROT_WRITE, MAP_SHARED, fd, (count - 1) * mapsize);
      recv(filemap, mapsize);
      munmap(filemap, mapsize);
      count ++;
   }
   if (size - mapsize * (count - 1) > 0)
   {
      filemap = (char *)mmap(0, size - mapsize * (count - 1), PROT_WRITE, MAP_SHARED, fd, (count - 1) * mapsize);
      recv(filemap, size - mapsize * (count - 1));
      munmap(filemap, size - mapsize * (count - 1));
   }
*/
   return size;
}

//
// The buffer magagement module. See recver.h
//

//
CSabulRecver::CBuffer::CBuffer():
m_lSize(40960000),
m_lStartPos(0),
m_lLastAckPos(0),
m_lMaxOffset(0),
m_pcUserBuf(NULL)
{
   m_pcData = new char [m_lSize];
}

CSabulRecver::CBuffer::CBuffer(const long& bufsize):
m_lSize(40960000),
m_lStartPos(0),
m_lLastAckPos(0),
m_lMaxOffset(0),
m_pcUserBuf(NULL)
{
   m_pcData = new char [m_lSize];
}

CSabulRecver::CBuffer::~CBuffer()
{
   delete [] m_pcData;
}

bool CSabulRecver::CBuffer::nextDataPos(char** data, long offset, const long& len)
{
   if (NULL != m_pcUserBuf)
   {
      if (m_lUserBufAck + offset + len <= m_lUserBufSize)
      {
         *data = m_pcUserBuf + m_lUserBufAck + offset;
         m_bAfterUserBufBoundary = false;
         return true;
      }
      else if (m_lUserBufAck + offset < m_lUserBufSize)
      {
         m_bAfterUserBufBoundary = false;
         return false;
      }
      else
         offset -= m_lUserBufSize - m_lUserBufAck;
   }

   m_bAfterUserBufBoundary = (NULL != m_pcUserBuf);
   long origoff = m_lMaxOffset;
   if (offset + len > m_lMaxOffset)
      m_lMaxOffset = offset+len;

   if (m_lLastAckPos >= m_lStartPos)
      if (m_lLastAckPos + offset + len <= m_lSize)
      {
         *data = m_pcData + m_lLastAckPos + offset;
         return true;
      }
      else if ((m_lLastAckPos + offset > m_lSize) && (offset - (m_lSize - m_lLastAckPos) + len <= m_lStartPos))
      {
         *data = m_pcData + offset - (m_lSize - m_lLastAckPos);
         return true;
      }

   if (m_lLastAckPos + offset + len <= m_lStartPos)
   {
      *data = m_pcData + m_lLastAckPos + offset;
      return true;
   }

   m_lMaxOffset = origoff;

   return false;
}

bool CSabulRecver::CBuffer::addData(char* data, long offset, long len)
{
   if (NULL != m_pcUserBuf)
   {
      if (m_lUserBufAck + offset + len <= m_lUserBufSize)
      {
         memcpy(m_pcUserBuf + m_lUserBufAck + offset, data, len);
         return true;
      }
      else if (m_lUserBufAck + offset < m_lUserBufSize)
      {
         memcpy(m_pcUserBuf + m_lUserBufAck + offset, data, m_lUserBufSize - (m_lUserBufAck + offset));
         data += m_lUserBufSize - (m_lUserBufAck + offset);
         len -= m_lUserBufSize - (m_lUserBufAck + offset);
         offset = 0;
      }
      else
         offset -= m_lUserBufSize - m_lUserBufAck;
   }

   long origoff = m_lMaxOffset;
   if (offset + len > m_lMaxOffset)
      m_lMaxOffset = offset + len;

   if (m_lLastAckPos >= m_lStartPos)
      if (m_lLastAckPos + offset + len <= m_lSize)
      {
         memcpy(m_pcData + m_lLastAckPos + offset, data, len);
         return true;
      }
      else if ((m_lLastAckPos + offset < m_lSize) && (len - (m_lSize - m_lLastAckPos - offset) <= m_lStartPos))
      {
         memcpy(m_pcData + m_lLastAckPos + offset, data, m_lSize - m_lLastAckPos - offset);
         memcpy(m_pcData, data + m_lSize - m_lLastAckPos - offset, len - (m_lSize - m_lLastAckPos - offset));
         return true;
      }
      else if ((m_lLastAckPos + offset >= m_lSize) && (offset - (m_lSize - m_lLastAckPos) + len <= m_lStartPos))
      {
         memcpy(m_pcData + offset - (m_lSize - m_lLastAckPos), data, len);
         return true;
      }

   if (m_lLastAckPos + offset + len <= m_lStartPos)
   {
      memcpy(m_pcData + m_lLastAckPos + offset, data, len);
      return true;
   }

   m_lMaxOffset = origoff;

   return false;
}

void CSabulRecver::CBuffer::moveData(const long& offset, const long& len)
{
   if (m_lMaxOffset - offset < len)
      return;

   if (m_lLastAckPos + m_lMaxOffset <= m_lSize)
      memmove(m_pcData + m_lLastAckPos + offset, m_pcData + m_lLastAckPos + offset + len, m_lMaxOffset - offset - len);
   else if (m_lLastAckPos + offset > m_lSize)
      memmove(m_pcData + (m_lLastAckPos + offset) % m_lSize, m_pcData + (m_lLastAckPos + offset + len) % m_lSize, m_lMaxOffset - offset - len);
   else if (m_lLastAckPos + offset + len <= m_lSize)
   {
      memmove(m_pcData + m_lLastAckPos + offset, m_pcData + m_lLastAckPos + offset + len, m_lSize - m_lLastAckPos - offset - len);
      memmove(m_pcData + m_lSize - len, m_pcData, len);
      memmove(m_pcData, m_pcData + len, m_lLastAckPos + m_lMaxOffset - m_lSize - len);
   }
   else
   {
      memmove(m_pcData + m_lLastAckPos + offset, m_pcData + len - (m_lSize - m_lLastAckPos - offset), m_lSize - m_lLastAckPos - offset);
      memmove(m_pcData, m_pcData + len, m_lLastAckPos + m_lMaxOffset - m_lSize - len);
   }
   m_lMaxOffset -= len;
}

bool CSabulRecver::CBuffer::readBuffer(char* data, const long& len)
{
   if (m_lStartPos + len <= m_lLastAckPos)
   {
      memcpy(data, m_pcData + m_lStartPos, len);
      m_lStartPos += len;
      return true;
   } 
   else if (m_lLastAckPos < m_lStartPos)
   { 
      if (m_lStartPos + len < m_lSize)
      {
         memcpy(data, m_pcData + m_lStartPos, len);
         m_lStartPos += len;
         return true;
      }
      if (len - (m_lSize - m_lStartPos) <= m_lLastAckPos)
      {
         memcpy(data, m_pcData + m_lStartPos, m_lSize - m_lStartPos);
         memcpy(data + m_lSize - m_lStartPos, m_pcData, len - (m_lSize - m_lStartPos));
         m_lStartPos = len - (m_lSize - m_lStartPos);
         return true;
      }
   }
   return false;
}

int CSabulRecver::CBuffer::ackData(const long& len)
{
   int ret = 0;

   if (NULL != m_pcUserBuf)
      if (m_lUserBufAck + len < m_lUserBufSize)
      {
         m_lUserBufAck += len;
         return ret;
      }
      else
      {
         m_lLastAckPos += m_lUserBufAck + len - m_lUserBufSize;
         m_lMaxOffset -= m_lUserBufAck + len - m_lUserBufSize;
         m_pcUserBuf = NULL;
         ret = 1;
      }
   else
   {
      m_lLastAckPos += len;
      m_lMaxOffset -= len;
   }
   m_lLastAckPos %= m_lSize;

   return ret;
}

void CSabulRecver::CBuffer::registerUserBuf(char* buf, const long& len)
{
   m_lUserBufAck = 0;
   m_lUserBufSize = len;
   m_pcUserBuf = buf;
   m_bAfterUserBufBoundary = false;

   long currwritepos = (m_lLastAckPos + m_lMaxOffset) % m_lSize;

   if (m_lStartPos <= currwritepos)
      if (currwritepos - m_lStartPos <= len)
      {
         memcpy(m_pcUserBuf, m_pcData + m_lStartPos, currwritepos - m_lStartPos);
         m_lMaxOffset = 0;
      }
      else
      {
         memcpy(m_pcUserBuf, m_pcData + m_lStartPos, len);
         m_lMaxOffset -= len;
      }
   else
      if (m_lSize - (m_lStartPos - currwritepos) <= len)
      {
         memcpy(m_pcUserBuf, m_pcData + m_lStartPos, m_lSize - m_lStartPos);
         memcpy(m_pcUserBuf + m_lSize - m_lStartPos, m_pcData, currwritepos);
         m_lMaxOffset = 0;
      }
      else
      {
         if (m_lSize - m_lStartPos <= len)
         {
            memcpy(m_pcUserBuf, m_pcData + m_lStartPos, m_lSize - m_lStartPos);
            memcpy(m_pcUserBuf + m_lSize - m_lStartPos, m_pcData, len - (m_lSize - m_lStartPos));
         }
         else
            memcpy(m_pcUserBuf, m_pcData + m_lStartPos, len);
         m_lMaxOffset -= len;
      }

   if (m_lStartPos <= m_lLastAckPos)
      m_lUserBufAck += m_lLastAckPos - m_lStartPos;
   else
      m_lUserBufAck += m_lSize - m_lStartPos + m_lLastAckPos;
 
   m_lStartPos = (m_lStartPos + len) % m_lSize;
   m_lLastAckPos = m_lStartPos;
}

bool CSabulRecver::CBuffer::reachUserBufBoundary()
{
   return m_bAfterUserBufBoundary;
}

long CSabulRecver::CBuffer::getSizeLimit() const
{
   return m_lSize;
}

// 
// The irregular packet list, see recver.h
//

//
CSabulRecver::CIrregularPktList::CIrregularPktList():
m_lSeqNo(0),
m_iErrorSize(0),
m_next(NULL)
{
}

CSabulRecver::CIrregularPktList::~CIrregularPktList()
{
}

long CSabulRecver::CIrregularPktList::currErrorSize() const
{
   return m_iErrorSize;
}

long CSabulRecver::CIrregularPktList::currErrorSize(const long& seqno) const
{
   if (NULL == m_next)
      return 0;

   long size = 0;
   CIrregularPktList *p = m_next;

   while ((NULL != p) && (((p->m_lSeqNo <= seqno) && (seqno - p->m_lSeqNo < (1 << 20))) || (p->m_lSeqNo - seqno > (1 << 20))))
   {
      size += p->m_iErrorSize;
      p = p->m_next;
   }

   return size;
}

void CSabulRecver::CIrregularPktList::addIrregularPkt(const long& seqno, const int& errsize)
{
   CIrregularPktList *p = const_cast<CIrregularPktList *>(this);

   while ((NULL != p->m_next) && (((p->m_lSeqNo <= seqno) && (seqno - p->m_lSeqNo < (1 << 20))) || (p->m_lSeqNo - seqno > (1 << 20))))
      p = p->m_next;
   
   CIrregularPktList *q = p->m_next; 
   p->m_next = new CIrregularPktList;
   p = p->m_next;
   p->m_lSeqNo = seqno;
   p->m_iErrorSize = errsize;
   p->m_next = q;

   m_iErrorSize += errsize;
}

void CSabulRecver::CIrregularPktList::deleteIrregularPkt(const long& seqno)
{
   CIrregularPktList *p = m_next;
   CIrregularPktList *q = m_next;

   while ((NULL != p) && (((p->m_lSeqNo <= seqno) && (seqno - p->m_lSeqNo < (1 << 20))) || (p->m_lSeqNo - seqno > (1 << 20))))
   {
      p = p->m_next;
      m_iErrorSize -= q->m_iErrorSize;
      delete q;
      q = p;
   }

   m_next = p;
}

//
// The receiver side loss list, see recver.h
//

//
CSabulRecver::CLossList::CLossList():
m_lAttr(0),
m_next(NULL)
{
   m_tail = this;
}

CSabulRecver::CLossList::CLossList(const long& seqno):
m_lAttr(seqno),
m_next(NULL)
{
   gettimeofday(&m_LastFeedbackTime, 0);
}

void CSabulRecver::CLossList::insert(const long& seqno)
{
   CLossList *p = new CLossList(seqno);
   
   m_tail->m_next = p;
   m_tail = p;

   m_lAttr ++;
}

void CSabulRecver::CLossList::remove(const long& seqno)
{
   CLossList *p = this;

   while ((NULL != p->m_next) && (((p->m_next->m_lAttr < seqno) && (seqno - p->m_next->m_lAttr < (1 << 20))) || (p->m_lAttr - seqno > (1 << 20))))
      p = p->m_next;

   if ((NULL != p->m_next) && (p->m_next->m_lAttr == seqno))
   {
      CLossList *q = p->m_next;
      p->m_next = q->m_next;
      if (m_tail == q)
         m_tail = p;
      delete q;
      m_lAttr --;
   }
}

int CSabulRecver::CLossList::getLossLength() const
{
   return m_lAttr;
}

long CSabulRecver::CLossList::getFirstLostSeq() const
{
   return (NULL == m_next) ? -1 : m_next->m_lAttr;
}

void CSabulRecver::CLossList::getLossArray(long* array, int& len, const int& limit, const long& interval) const
{
   CLossList *p = this->m_next;

   timeval currtime;
   gettimeofday(&currtime, 0);

   for (len = 0; (NULL != p) && (len < limit); p = p->m_next)
      if ((currtime.tv_sec - p->m_LastFeedbackTime.tv_sec) * 1000000 + currtime.tv_usec - p->m_LastFeedbackTime.tv_usec > interval)
      {
         array[len] = p->m_lAttr;
         gettimeofday(&(p->m_LastFeedbackTime), 0);
         len ++;
      }
}

//

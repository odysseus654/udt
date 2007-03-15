/*****************************************************************************
Copyright © 2001 - 2007, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) special version UDT-m

National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.ncdm.uic.edu/

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
This file contains the implementation of UDT sending and receiving buffer
management modules.

The sending buffer is a linked list of application data to be sent.
The receiving buffer is a logically circular memeory block.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 01/29/2007
*****************************************************************************/

#include <cstring>
#include <cmath>
#include "buffer.h"
#include <iostream>
using namespace std;

CSndBuffer::CSndBuffer(const int& mss):
m_pBlock(NULL),
m_pLastBlock(NULL),
m_pCurrSendBlk(NULL),
m_pCurrAckBlk(NULL),
m_iCurrBufSize(0),
m_iCurrSendPnt(0),
m_iCurrAckPnt(0),
m_iNextMsgNo(0),
m_iMSS(mss)
{
   #ifndef WIN32
      pthread_mutex_init(&m_BufLock, NULL);
   #else
      m_BufLock = CreateMutex(NULL, false, NULL);
   #endif
}

CSndBuffer::~CSndBuffer()
{
   Block* pb = m_pBlock;

   // Release allocated data structure if there is any
   while (NULL != m_pBlock)
   {
      pb = pb->m_next;

      // process user data according with the routine provided by applications
      if (NULL != m_pBlock->m_pMemRoutine)
         m_pBlock->m_pMemRoutine(m_pBlock->m_pcData, m_pBlock->m_iLength, m_pBlock->m_pContext);

      delete m_pBlock;
      m_pBlock = pb;
   }

   #ifndef WIN32
      pthread_mutex_destroy(&m_BufLock);
   #else
      CloseHandle(m_BufLock);
   #endif
}

void CSndBuffer::addBuffer(const char* data, const int& len, const int& handle, const UDT_MEM_ROUTINE func, void* context, const int& ttl, const int32_t& seqno, const bool& order)
{
   CGuard bufferguard(m_BufLock);

   if (NULL == m_pBlock)
   {
      // Insert a block to the empty list   
  
      m_pBlock = new Block;
      m_pBlock->m_pcData = const_cast<char *>(data);
      m_pBlock->m_iLength = len;
      m_pBlock->m_OriginTime = CTimer::getTime();
      m_pBlock->m_iTTL = ttl;
      m_pBlock->m_iMsgNo = m_iNextMsgNo;
      m_pBlock->m_iSeqNo = seqno;
      m_pBlock->m_iInOrder = order;
      m_pBlock->m_iInOrder <<= 29;
      m_pBlock->m_iHandle = handle;
      m_pBlock->m_pMemRoutine = func;
      m_pBlock->m_pContext = context;
      m_pBlock->m_next = NULL;
      m_pLastBlock = m_pBlock;
      m_pCurrSendBlk = m_pBlock;
      m_iCurrSendPnt = 0;
      m_pCurrAckBlk = m_pBlock;
      m_iCurrAckPnt = 0;
   }
   else
   {
      // Insert a new block to the tail of the list

      int32_t lastseq = m_pLastBlock->m_iSeqNo;
      int offset = m_pLastBlock->m_iLength;

      m_pLastBlock->m_next = new Block;
      m_pLastBlock = m_pLastBlock->m_next;
      m_pLastBlock->m_pcData = const_cast<char *>(data);
      m_pLastBlock->m_iLength = len;
      m_pLastBlock->m_OriginTime = CTimer::getTime();
      m_pLastBlock->m_iTTL = ttl;
      m_pLastBlock->m_iMsgNo = m_iNextMsgNo;
      m_pLastBlock->m_iSeqNo = lastseq + (int32_t)ceil(double(offset) / m_iMSS);
      m_pLastBlock->m_iInOrder = order;
      m_pLastBlock->m_iInOrder <<= 29;
      m_pLastBlock->m_iHandle = handle;
      m_pLastBlock->m_pMemRoutine = func;
      m_pLastBlock->m_pContext = context;
      m_pLastBlock->m_next = NULL;
      if (NULL == m_pCurrSendBlk)
         m_pCurrSendBlk = m_pLastBlock;
   }

   m_iNextMsgNo = CMsgNo::incmsg(m_iNextMsgNo);

   m_iCurrBufSize += len;
}

int CSndBuffer::readData(char** data, const int& len, int32_t& msgno)
{
   CGuard bufferguard(m_BufLock);

   // No data to read
   if (NULL == m_pCurrSendBlk)
      return 0;

   // read data in the current sending block
   if (m_iCurrSendPnt + len < m_pCurrSendBlk->m_iLength)
   {
      *data = m_pCurrSendBlk->m_pcData + m_iCurrSendPnt;

      msgno = m_pCurrSendBlk->m_iMsgNo | m_pCurrSendBlk->m_iInOrder;
      if (0 == m_iCurrSendPnt)
         msgno |= 0x80000000;
      if (m_pCurrSendBlk->m_iLength == m_iCurrSendPnt + len)
         msgno |= 0x40000000;

      m_iCurrSendPnt += len;

      return len;
   }

   // Not enough data to read. 
   // Read an irregular packet and move the current sending block pointer to the next block
   int readlen = m_pCurrSendBlk->m_iLength - m_iCurrSendPnt;
   *data = m_pCurrSendBlk->m_pcData + m_iCurrSendPnt;

   if (0 == m_iCurrSendPnt)
      msgno = m_pCurrSendBlk->m_iMsgNo | 0xC0000000 | m_pCurrSendBlk->m_iInOrder;
   else
      msgno = m_pCurrSendBlk->m_iMsgNo | 0x40000000 | m_pCurrSendBlk->m_iInOrder;

   m_pCurrSendBlk = m_pCurrSendBlk->m_next;
   m_iCurrSendPnt = 0;

   return readlen;
}

int CSndBuffer::readData(char** data, const int offset, const int& len, int32_t& msgno, int32_t& seqno, int& msglen)
{
   CGuard bufferguard(m_BufLock);

   Block* p = m_pCurrAckBlk;

   // No data to read
   if (NULL == p)
      return 0;

   // Locate to the data position by the offset
   int loffset = offset + m_iCurrAckPnt;
   while (p->m_iLength <= loffset)
   {
      loffset -= p->m_iLength;
      loffset -= len - ((0 == p->m_iLength % len) ? len : (p->m_iLength % len));
      p = p->m_next;
      if (NULL == p)
         return 0;
   }

   if (p->m_iTTL >= 0)
   {
      if (int(CTimer::getTime() - p->m_OriginTime) > p->m_iTTL)
      {
         msgno = p->m_iMsgNo;
         seqno = p->m_iSeqNo;
         msglen = p->m_iLength;

         return -1;
      }
   }

   // Read a regular data
   if (loffset + len <= p->m_iLength)
   {
      *data = p->m_pcData + loffset;
      msgno = p->m_iMsgNo | p->m_iInOrder;

      if (0 == loffset)
         msgno |= 0x80000000;
      if (p->m_iLength == loffset + len)
         msgno |= 0x40000000;

      return len;
   }

   // Read an irrugular data at the end of a block
   *data = p->m_pcData + loffset;
   msgno = p->m_iMsgNo | p->m_iInOrder;

   if (0 == loffset)
      msgno |= 0xC0000000;
   else
      msgno |= 0x40000000;

   return p->m_iLength - loffset;
}

void CSndBuffer::ackData(const int& len, const int& payloadsize)
{
   CGuard bufferguard(m_BufLock);

   m_iCurrAckPnt += len;

   // Remove the block if it is acknowledged
   while (m_iCurrAckPnt >= m_pCurrAckBlk->m_iLength)
   {
      m_iCurrAckPnt -= m_pCurrAckBlk->m_iLength;

      // Update the size error between regular and irregular packets
      if (0 != m_pCurrAckBlk->m_iLength % payloadsize)
         m_iCurrAckPnt -= payloadsize - m_pCurrAckBlk->m_iLength % payloadsize;

      m_iCurrBufSize -= m_pCurrAckBlk->m_iLength;
      m_pCurrAckBlk = m_pCurrAckBlk->m_next;

      // process user data according with the routine provided by applications
      if (NULL != m_pBlock->m_pMemRoutine)
         m_pBlock->m_pMemRoutine(m_pBlock->m_pcData, m_pBlock->m_iLength, m_pBlock->m_pContext);

      delete m_pBlock;
      m_pBlock = m_pCurrAckBlk;

      if (NULL == m_pBlock)
         break;
   }
}

int CSndBuffer::getCurrBufSize() const
{
   return m_iCurrBufSize - m_iCurrAckPnt;
}

bool CSndBuffer::getOverlappedResult(const int& handle, int& progress)
{
   CGuard bufferguard(m_BufLock);

   if (NULL != m_pCurrAckBlk)
   {
      if (handle == m_pCurrAckBlk->m_iHandle)
      {
         progress = m_iCurrAckPnt;
         return false;
      }
      else 
      {
         if (((m_pLastBlock->m_iHandle < m_pCurrAckBlk->m_iHandle) && (handle < m_pCurrAckBlk->m_iHandle) && (m_pLastBlock->m_iHandle <= handle))
            || ((m_pLastBlock->m_iHandle > m_pCurrAckBlk->m_iHandle) && ((handle < m_pCurrAckBlk->m_iHandle) || (m_pLastBlock->m_iHandle <= handle))))
         {
            progress = 0;
            return false;
         }
      }
   }

   progress = 0;
   return true;
}

void CSndBuffer::releaseBuffer(char* buf, int, void*)
{
   delete [] buf;
}

////////////////////////////////////////////////////////////////////////////////

CRcvBuffer::CRcvBuffer(CUnitQueue* queue):
m_pUnit(NULL),
m_iSize(65536),
m_pUnitQueue(queue),
m_iStartPos(0),
m_iLastAckPos(0),
m_pMessageList(NULL)
{
   m_pUnit = new CUnit* [m_iSize];

   #ifndef WIN32
      pthread_mutex_init(&m_MsgLock, NULL);
   #else
      m_MsgLock = CreateMutex(NULL, false, NULL);
   #endif
}

CRcvBuffer::CRcvBuffer(const int& bufsize, CUnitQueue* queue):
m_pUnit(NULL),
m_iSize(bufsize),
m_pUnitQueue(queue),
m_iStartPos(0),
m_iLastAckPos(0),
m_pMessageList(NULL)
{
   m_pUnit = new CUnit* [m_iSize];
   for (int i = 0; i < m_iSize; ++ i)
      m_pUnit[i] = NULL;

   #ifndef WIN32
      pthread_mutex_init(&m_MsgLock, NULL);
   #else
      m_MsgLock = CreateMutex(NULL, false, NULL);
   #endif
}

CRcvBuffer::~CRcvBuffer()
{
   for (int i = 0; i < m_iSize; ++ i)
   {
      if (NULL != m_pUnit[i])
      {
         m_pUnit[i]->m_bValid = false;
         m_pUnitQueue->m_iCount --;
      }
   }

   delete [] m_pUnit;

   if (NULL != m_pMessageList)
      delete [] m_pMessageList;

   #ifndef WIN32
      pthread_mutex_destroy(&m_MsgLock);
   #else
      CloseHandle(m_MsgLock);
   #endif
}

void CRcvBuffer::addData(CUnit* unit, int offset)
{
   int pos = (m_iLastAckPos + offset) % m_iSize;
   m_pUnit[pos] = unit;

//cout << "ADD " << pos << endl;
}

int CRcvBuffer::readBuffer(char* data, const int& len)
{
   // empty buffer
   if (m_iStartPos == m_iLastAckPos)
      return 0;

   int p = m_iStartPos;
   int rs = len;

   int unitsize = m_pUnit[p]->m_Packet.getLength() - m_iNotch;
   if (rs >= unitsize)
   {
      memcpy(data, m_pUnit[p]->m_Packet.m_pcData + m_iNotch, unitsize);
      data += unitsize;

      m_pUnit[p]->m_bValid = false;
      m_pUnitQueue->m_iCount --;
      m_pUnit[p] = NULL;

      m_iNotch = 0;
      rs -= unitsize;

      if (++ p == m_iSize)
         p = 0;
   }
   else
   {
      memcpy(data, m_pUnit[p]->m_Packet.m_pcData + m_iNotch, rs);
      m_iNotch += rs;
      return rs;
   }

   while ((p != m_iLastAckPos) && (rs > 0))
   {
      unitsize = m_pUnit[p]->m_Packet.getLength();
      if (rs >= unitsize)
      {
//cout << "** " << p << " " << m_pUnit[p]->m_bValid << " " << unitsize << " " << m_iStartPos << " " << m_iLastAckPos << " " << m_iSize << endl;
         memcpy(data, m_pUnit[p]->m_Packet.m_pcData, unitsize);
         data += unitsize;

         m_pUnit[p]->m_bValid = false;
         m_pUnitQueue->m_iCount --;
         m_pUnit[p] = NULL;

         rs -= unitsize;

         if (++ p == m_iSize)
            p = 0;
      }
      else
      {
         memcpy(data, m_pUnit[p]->m_Packet.m_pcData, rs);
         m_iNotch = rs;
         rs = 0;
      }
   }

   m_iStartPos = p;

   return len - rs;
}

void CRcvBuffer::ackData(const int& len)
{
   m_iLastAckPos = (m_iLastAckPos + len) % m_iSize;

   //cout << "BUF ACK " << m_iStartPos << " " << m_iLastAckPos << endl;
}

int CRcvBuffer::getAvailBufSize() const
{
   return m_iSize - getRcvDataSize();
}

int CRcvBuffer::getRcvDataSize() const
{
   if (m_iLastAckPos >= m_iStartPos)
      return m_iLastAckPos - m_iStartPos;

   return m_iSize + m_iLastAckPos - m_iStartPos;
}


void CRcvBuffer::initMsgList()
{
   // the message list should contain the maximum possible number of messages: when each packet is a message
   m_iMsgInfoSize = m_iSize / m_iMSS + 1;

   m_pMessageList = new MsgInfo[m_iMsgInfoSize];

   m_iPtrFirstMsg = -1;
   m_iPtrRecentACK = -1;
   m_iLastMsgNo = 0;
   m_iValidMsgCount = 0;

   for (int i = 0; i < m_iMsgInfoSize; ++ i)
   {
      m_pMessageList[i].m_pcData = NULL;
      m_pMessageList[i].m_iMsgNo = -1;
      m_pMessageList[i].m_iStartSeq = -1;
      m_pMessageList[i].m_iEndSeq = -1;
      m_pMessageList[i].m_iSizeDiff = 0;
      m_pMessageList[i].m_bValid = false;
      m_pMessageList[i].m_bDropped = false;
      m_pMessageList[i].m_bInOrder = false;
      m_pMessageList[i].m_iMsgNo = -1;
   }
}

void CRcvBuffer::checkMsg(const int& type, const int32_t& msgno, const int32_t& seqno, const char* ptr, const bool& inorder, const int& diff)
{
   CGuard msgguard(m_MsgLock);

   int pos;

   if (-1 == m_iPtrFirstMsg)
   {
      pos = m_iPtrFirstMsg = 0;
      m_iPtrRecentACK = -1;
   }
   else
   {
      pos = CMsgNo::msgoff(m_pMessageList[m_iPtrFirstMsg].m_iMsgNo, msgno);

      if (pos >= 0)
         pos = (m_iPtrFirstMsg + pos) % m_iMsgInfoSize;
      else
      {
         pos = (m_iPtrFirstMsg + pos + m_iMsgInfoSize) % m_iMsgInfoSize;
         m_iPtrFirstMsg = pos;
      }
   }

   MsgInfo* p = m_pMessageList + pos;

   p->m_iMsgNo = msgno;

   switch (type)
   {
   case 3: // 11
      // single packet message
      p->m_pcData = (char*)ptr;
      p->m_iStartSeq = p->m_iEndSeq = seqno;
      p->m_bInOrder = inorder;
      p->m_iSizeDiff = diff;

      break;

   case 2: // 10
      // first packet of the message
      p->m_pcData = (char*)ptr;
      p->m_iStartSeq = seqno;
      p->m_bInOrder = inorder;

      break;

   case 1: // 01
      // last packet of the message
      p->m_iEndSeq = seqno;
      p->m_iSizeDiff = diff;

      break;
   }

   // update the largest msg no so far
   if (CMsgNo::msgcmp(m_iLastMsgNo, msgno) < 0)
      m_iLastMsgNo = msgno;
}

bool CRcvBuffer::ackMsg(const int32_t& ack, const CRcvLossList* rll)
{
   CGuard msgguard(m_MsgLock);

   return (m_iValidMsgCount > 0);
}

void CRcvBuffer::dropMsg(const int32_t& msgno)
{
   CGuard msgguard(m_MsgLock);
}

int CRcvBuffer::readMsg(char* data, const int& len)
{
   CGuard msgguard(m_MsgLock);

   return 0;
}

int CRcvBuffer::getValidMsgCount()
{
   CGuard msgguard(m_MsgLock);

   return m_iValidMsgCount;
}

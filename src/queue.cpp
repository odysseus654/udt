/*****************************************************************************
Copyright � 2001 - 2007, The Board of Trustees of the University of Illinois.
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
This file contains the implementation of UDT multiplexer.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 03/13/2007
*****************************************************************************/

#include "common.h"
#include "queue.h"
#include "core.h"
#include <iostream>
using namespace std;

CUnitQueue::CUnitQueue():
m_iSize(0),
m_iCount(0)
{
   m_vpUnit.clear();
   m_vpBuffer.clear();
   m_vpAddrBuf.clear();
   m_viSize.clear();
}

CUnitQueue::~CUnitQueue()
{
   for (vector<CUnit*>::iterator i = m_vpUnit.begin(); i != m_vpUnit.end(); ++ i)
      delete [] *i;
   m_vpUnit.clear();

   for (vector<char*>::iterator i = m_vpBuffer.begin(); i != m_vpBuffer.end(); ++ i)
      delete [] *i;
   m_vpBuffer.clear();

   for (vector<char*>::iterator i = m_vpAddrBuf.begin(); i != m_vpAddrBuf.end(); ++ i)
      delete [] *i;
   m_vpAddrBuf.clear();

   m_viSize.clear();
}

int CUnitQueue::init(const int& size, const int& mss)
{
   CUnit* tempu = NULL;
   char* tempb = NULL;
   char* tempa = NULL;

   try
   {
      tempu = new CUnit [size];
      tempb = new char [size * mss];
      tempa = (char*) new sockaddr_in [size];
   }
   catch (...)
   {
      delete [] tempu;
      delete [] tempb;
      delete [] tempa;

      return -1;
   }

   for (int i = 0; i < size; ++ i)
   {
      tempu[i].m_bValid = false;
      tempu[i].m_pAddr = (sockaddr*)((sockaddr_in*)tempa + i);
      tempu[i].m_Packet.m_pcData = tempb + i * mss;
   }

   m_vpUnit.insert(m_vpUnit.end(), tempu);
   m_vpBuffer.insert(m_vpBuffer.end(), tempb);
   m_vpAddrBuf.insert(m_vpAddrBuf.end(), tempa);
   m_viSize.insert(m_viSize.end(), size);

   m_iSize = size;
   m_iMSS = mss;

   m_pAvailUnit = (CUnit*)m_vpUnit[0];
   m_iVQ = 0;

   return 0;
}

int CUnitQueue::increase()
{
cout << "INCREASE!!!! " << m_iCount << " " << m_iSize << endl;

   CUnit* tempu = NULL;
   char* tempb = NULL;
   char* tempa = NULL;

   try
   {
      tempu = new CUnit [m_iSize];
      tempb = new char [m_iSize * m_iMSS];
      tempa = (char*) new sockaddr_in [m_iSize];
   }
   catch (...)
   {
      delete [] tempu;
      delete [] tempb;
      delete [] tempa;

      return -1;
   }

   for (int i = 0; i < m_iSize; ++ i)
   {
      tempu[i].m_bValid = false;
      tempu[i].m_pAddr = (sockaddr*)((sockaddr_in*)tempa + i);
      tempu[i].m_Packet.m_pcData = tempb + i * m_iMSS;
   }

   m_vpUnit.insert(m_vpUnit.end(), tempu);
   m_vpBuffer.insert(m_vpBuffer.end(), tempb);
   m_vpAddrBuf.insert(m_vpAddrBuf.end(), tempa);
   m_viSize.insert(m_viSize.end(), m_iSize);

   m_iSize *= 2;
   return 0;
}

int CUnitQueue::shrink()
{
   // currently queue cannot be shrinked.
   return -1;
}

CUnit* CUnitQueue::getNextAvailUnit()
{
   if (double(m_iCount) / m_iSize > 0.9)
      increase();

   if (m_iCount == m_iSize)
      return NULL;

   while (true)
   {
      for (CUnit* sentinel = (CUnit*)m_vpUnit[m_iVQ] + m_viSize[m_iVQ] - 1; m_pAvailUnit != sentinel; ++ m_pAvailUnit)
         if (!m_pAvailUnit->m_bValid)
            return m_pAvailUnit;

      if (m_iVQ != int(m_vpUnit.size() - 1))
         m_pAvailUnit = (CUnit*)m_vpUnit[++ m_iVQ];
      else
      {
         m_iVQ = 0;
         m_pAvailUnit = (CUnit*)m_vpUnit[0];
      }
   }

   return NULL;
}


CSndUList::CSndUList():
m_pUList(NULL),
m_pLast(NULL)
{
   #ifndef WIN32
      pthread_mutex_init(&m_ListLock, NULL);
   #else
      m_ListLock = CreateMutex(NULL, false, NULL);
   #endif
}

CSndUList::~CSndUList()
{
#ifndef WIN32
   pthread_mutex_destroy(&m_ListLock);
#else
   CloseHandle(m_ListLock);
#endif
}

void CSndUList::insert(const int64_t& ts, const int32_t& id, const CUDT* u)
{
   CGuard listguard(m_ListLock);

   CUDTList* n = u->m_pSNode;
   n->m_llTimeStamp = ts;

   if (NULL == m_pUList)
   {
      n->m_pPrev = n->m_pNext = NULL;
      m_pLast = m_pUList = n;

      // If UList was empty, signal the sending queue to restart
      #ifndef WIN32
         pthread_mutex_lock(m_pWindowLock);
         pthread_cond_signal(m_pWindowCond);
         pthread_mutex_unlock(m_pWindowLock);
      #else
         SetEvent(*m_pWindowCond);
      #endif

      return;
   }

   // SndUList is sorted by the next processing time

   if (n->m_llTimeStamp >= m_pLast->m_llTimeStamp)
   {
      // do not insert repeated node
      if (id == m_pLast->m_iID)
         return;

      // insert as the last node
      n->m_pPrev = m_pLast;
      n->m_pNext = NULL;
      m_pLast->m_pNext = n;
      m_pLast = n;

      return;
   }
   else if (n->m_llTimeStamp <= m_pUList->m_llTimeStamp)
   {
      // do not insert repeated node
      if (id == m_pUList->m_iID)
         return;

      // insert as the first node
      n->m_pPrev = NULL;
      n->m_pNext = m_pUList;
      m_pUList->m_pPrev = n;
      m_pUList = n;

      return;
   }

   // check somewhere in the middle
   CUDTList* p = m_pLast->m_pPrev;
   while (p->m_llTimeStamp > n->m_llTimeStamp)
      p = p->m_pPrev;

   // do not insert repeated node
   if ((id == p->m_iID) || (id == p->m_pNext->m_iID) || ((NULL != p->m_pPrev) && (id == p->m_pPrev->m_iID)))
      return;

   n->m_pPrev = p;
   n->m_pNext = p->m_pNext;
   p->m_pNext->m_pPrev = n;
   p->m_pNext = n;
}

void CSndUList::remove(const int32_t& id)
{
   CGuard listguard(m_ListLock);

   if (NULL == m_pUList)
      return;

   if (id == m_pUList->m_iID)
   {
      // check and remove the first node
      m_pUList = m_pUList->m_pNext;
      if (NULL == m_pUList)
         m_pLast = NULL;
      else
         m_pUList->m_pPrev = NULL;

      return;
   }

   // check further
   CUDTList* p = m_pUList->m_pNext;
   while (NULL != p)
   {
      if (id == p->m_iID)
      {
         p->m_pPrev->m_pNext = p->m_pNext;
         if (NULL != p->m_pNext)
            p->m_pNext->m_pPrev = p->m_pPrev;
         else
            m_pLast = p->m_pPrev;

         return;
      }

      p = p->m_pNext;
   }
}

void CSndUList::update(const int32_t& id, const CUDT* u, const bool& reschedule)
{
   CGuard listguard(m_ListLock);

   if (NULL == m_pUList)
   {
      // insert a new entry if the list was empty
      CUDTList* n = u->m_pSNode;
      n->m_llTimeStamp = 1;
      n->m_pPrev = n->m_pNext = NULL;
      m_pLast = m_pUList = n;

      #ifndef WIN32
         pthread_mutex_lock(m_pWindowLock);
         pthread_cond_signal(m_pWindowCond);
         pthread_mutex_unlock(m_pWindowLock);
      #else
         SetEvent(*m_pWindowCond);
      #endif

      return;
   }

   if (id == m_pUList->m_iID)
   {
      if (reschedule)
         m_pUList->m_llTimeStamp = 1;
      return;
   }

   // remove the old entry
   CUDTList* p = m_pUList->m_pNext;
   while (NULL != p)
   {
      if (id == p->m_iID)
      {
         if (!reschedule)
            return;

         p->m_pPrev->m_pNext = p->m_pNext;
         if (NULL != p->m_pNext)
            p->m_pNext->m_pPrev = p->m_pPrev;
         else
            m_pLast = p->m_pPrev;

         break;
      }

      p = p->m_pNext;
   }

   // insert at head
   CUDTList* n = u->m_pSNode;
   n->m_llTimeStamp = 1;
   n->m_pPrev = NULL;
   n->m_pNext = m_pUList;
   m_pUList->m_pPrev = n;
   m_pUList = n;
}

int CSndUList::pop(int32_t& id, CUDT*& u)
{
   CGuard listguard(m_ListLock);

   if (NULL == m_pUList)
      return -1;

   id = m_pUList->m_iID;
   u = m_pUList->m_pUDT;

   m_pUList = m_pUList->m_pNext;
   if (NULL == m_pUList)
      m_pLast = NULL;
   else
      m_pUList->m_pPrev = NULL;

   return id;
}


//
CSndQueue::CSndQueue():
m_pUnitQueue(NULL),
m_iQueueLen(0),
m_iHeadPtr(0),
m_iTailPtr(0),
m_pSndUList(NULL),
m_pChannel(NULL),
m_pTimer(NULL)
{
   #ifndef WIN32
      pthread_cond_init(&m_QueueCond, NULL);
      pthread_mutex_init(&m_QueueLock, NULL);

      pthread_cond_init(&m_WindowCond, NULL);
      pthread_mutex_init(&m_WindowLock, NULL);
   #else
      m_QueueLock = CreateMutex(NULL, false, NULL);
      m_QueueCond = CreateEvent(NULL, false, false, NULL);

      m_WindowLock = CreateMutex(NULL, false, NULL);
      m_WindowCond = CreateEvent(NULL, false, false, NULL);
   #endif
}

CSndQueue::~CSndQueue()
{
   #ifndef WIN32
      pthread_cond_destroy(&m_QueueCond);
      pthread_mutex_destroy(&m_QueueLock);

      pthread_cond_destroy(&m_WindowCond);
      pthread_mutex_destroy(&m_WindowLock);
   #else
      CloseHandle(m_QueueLock);
      CloseHandle(m_QueueCond);

      CloseHandle(m_WindowLock);
      CloseHandle(m_WindowCond);
   #endif

   if (NULL == m_pUnitQueue)
      return;

   delete [] m_pUnitQueue;
}

void CSndQueue::init(const int& size, const CChannel* c, const CTimer* t)
{
   m_iQueueLen = size;
   m_pUnitQueue = new CUnit[size];
   m_pChannel = (CChannel*)c;
   m_pTimer = (CTimer*)t;
   m_pSndUList = new CSndUList;
   m_pSndUList->m_pWindowLock = &m_WindowLock;
   m_pSndUList->m_pWindowCond = &m_WindowCond;

   #ifndef WIN32
      pthread_create(&m_enQThread, NULL, CSndQueue::enQueue, this);
      pthread_detach(m_enQThread);
      pthread_create(&m_deQThread, NULL, CSndQueue::deQueue, this);
      pthread_detach(m_deQThread);
   #else
      DWORD threadID;
      m_enQThread = CreateThread(NULL, 0, CSndQueue::enQueue, this, 0, &threadID);
      m_deQThread = CreateThread(NULL, 0, CSndQueue::deQueue, this, 0, &threadID);
   #endif
}

#ifndef WIN32
   void* CSndQueue::enQueue(void* param)
#else
   DWORD WINAPI CSndQueue::enQueue(LPVOID param)
#endif
{
   CSndQueue* self = (CSndQueue*)param;

   while (true)
   {
      if (NULL != self->m_pSndUList->m_pUList)
      {
         // wait until next processing time of the first socket on the list
         uint64_t currtime;
         CTimer::rdtsc(currtime);
         if (currtime < self->m_pSndUList->m_pUList->m_llTimeStamp)
            self->m_pTimer->sleepto(self->m_pSndUList->m_pUList->m_llTimeStamp);

         // it is time to process it, pop it out/remove from the list
         int32_t id;
         CUDT* u;
         if (self->m_pSndUList->pop(id, u) < 0)
            continue;

         while ((self->m_iTailPtr + 1 == self->m_iHeadPtr) || ((self->m_iTailPtr == self->m_iQueueLen - 1) && (self->m_iHeadPtr == 0)))
         {
            // add pthread_cond_wait here
            #ifndef WIN32
               pthread_cond_signal(&self->m_QueueCond);
            #else
               SetEvent(self->m_QueueCond);
            #endif
         }

         // pack a packet from the socket
         uint64_t ts;
         int ps = u->packData(self->m_pUnitQueue[self->m_iTailPtr].m_Packet, ts);

         if (ps > 0)
         {
            // insert the packet to the sending queue
            self->m_pUnitQueue[self->m_iTailPtr].m_pAddr = u->m_pPeerAddr;

            if (self->m_iQueueLen != self->m_iTailPtr + 1)
               ++ self->m_iTailPtr;
            else
               self->m_iTailPtr = 0;

            // activate the dequeue process
            #ifndef WIN32
               pthread_mutex_lock(&self->m_QueueLock);
               if ((self->m_iHeadPtr + 1 == self->m_iTailPtr) || (0 == self->m_iTailPtr))
                  pthread_cond_signal(&self->m_QueueCond);
               pthread_mutex_unlock(&self->m_QueueLock);
            #else
               if ((self->m_iHeadPtr + 1 == self->m_iTailPtr) || (0 == self->m_iTailPtr))
                  SetEvent(self->m_QueueCond);
            #endif
         }

         // insert a new entry, ts is the next processing time
         if (ts > 0)
            self->m_pSndUList->insert(ts, id, u);
      }
      else
      {
         // wait here is there is no sockets with data to be sent
         #ifndef WIN32
            pthread_mutex_lock(&self->m_WindowLock);
            if (NULL == self->m_pSndUList->m_pUList)
               pthread_cond_wait(&self->m_WindowCond, &self->m_WindowLock);
            pthread_mutex_unlock(&self->m_WindowLock);
         #else
            WaitForSingleObject(self->m_WindowCond, INFINITE);
         #endif
      }
   }

   return NULL;
}

#ifndef WIN32
   void* CSndQueue::deQueue(void* param)
#else
   DWORD WINAPI CSndQueue::deQueue(LPVOID param)
#endif
{
   CSndQueue* self = (CSndQueue*)param;

   while (true)
   {
      if (self->m_iHeadPtr != self->m_iTailPtr)
      {
         // send the first packet on the queue
         self->m_pChannel->sendto(self->m_pUnitQueue[self->m_iHeadPtr].m_pAddr, self->m_pUnitQueue[self->m_iHeadPtr].m_Packet);

         // and remove it from the queue
         if (self->m_iQueueLen != self->m_iHeadPtr + 1)
            ++ self->m_iHeadPtr;
         else
            self->m_iHeadPtr = 0;
      }
      else
      {
         // no packet to be sent? wait here
         #ifndef WIN32
            pthread_mutex_lock(&self->m_QueueLock);
            if (self->m_iHeadPtr == self->m_iTailPtr)
               pthread_cond_wait(&self->m_QueueCond, &self->m_QueueLock);
            pthread_mutex_unlock(&self->m_QueueLock);
         #else
            WaitForSingleObject(self->m_QueueCond, 1);
         #endif
      }
   }

   return NULL;
}

int CSndQueue::sendto(const sockaddr* addr, CPacket& packet)
{
   // send out the packet immediately (high priority), this is a control packet
   m_pChannel->sendto(addr, packet);

   return packet.getLength();
}


//
CRcvUList::CRcvUList():
m_pUList(NULL),
m_pLast(NULL)
{
   #ifndef WIN32
      pthread_mutex_init(&m_ListLock, NULL);
   #else
      m_ListLock = CreateMutex(NULL, false, NULL);
   #endif
}

CRcvUList::~CRcvUList()
{
   #ifndef WIN32
      pthread_mutex_destroy(&m_ListLock);
   #else
      CloseHandle(m_ListLock);
   #endif
}

void CRcvUList::insert(const int32_t& id, const CUDT* u)
{
   CGuard listguard(m_ListLock);

   CUDTList* n = u->m_pRNode;
   CTimer::rdtsc(n->m_llTimeStamp);

   if (NULL == m_pUList)
   {
      // empty list, insert as the single node
      n->m_pPrev = n->m_pNext = NULL;
      m_pLast = m_pUList = n;

      return;
   }

   // always insert at the end for RcvUList
   n->m_pPrev = m_pLast;
   n->m_pNext = NULL;
   m_pLast->m_pNext = n;
   m_pLast = n;
}

void CRcvUList::remove(const int32_t& id)
{
   CGuard listguard(m_ListLock);

   if (NULL == m_pUList)
      return;

   if (id == m_pUList->m_iID)
   {
      // remove first node
      m_pUList = m_pUList->m_pNext;
      if (NULL == m_pUList)
         m_pLast = NULL;
      else
         m_pUList->m_pPrev = NULL;

      return;
   }

   // check further
   CUDTList* p = m_pUList;
   while (NULL != p->m_pNext)
   {
      if (id == p->m_pNext->m_iID)
      {
         p->m_pNext = p->m_pNext->m_pNext;
         if (NULL != p->m_pNext)
            p->m_pNext->m_pPrev = p;
         else
            m_pLast = p;

         return;
      }

      p = p->m_pNext;
   }
}


//
CHash::CHash()
{
   #ifndef WIN32
      pthread_mutex_init(&m_ListLock, NULL);
   #else
      m_ListLock = CreateMutex(NULL, false, NULL);
   #endif
}

CHash::~CHash()
{
   #ifndef WIN32
      pthread_mutex_destroy(&m_ListLock);
   #else
      CloseHandle(m_ListLock);
   #endif
}

void CHash::init(const int& size)
{
   m_pBucket = new CBucket* [size];

   for (int i = 0; i < size; ++ i)
      m_pBucket[i] = NULL;

   m_iHashSize = size;
}

CUDT* CHash::lookup(const int32_t& id)
{
   CGuard hashguard(m_ListLock);

   // simple hash function (% hash table size); suitable for socket descriptors
   CBucket* b = m_pBucket[id % m_iHashSize];

   while (NULL != b)
   {
      if (id == b->m_iID)
         return b->m_pUDT;
      b = b->m_pNext;
   }

   return NULL;
}

int CHash::retrieve(const int32_t& id, CPacket& packet)
{
   CGuard hashguard(m_ListLock);

   CBucket* b = m_pBucket[id % m_iHashSize];

   while (NULL != b)
   {
      if ((id == b->m_iID) && (NULL != b->m_pUnit))
      {
         memcpy(packet.m_nHeader, b->m_pUnit->m_Packet.m_nHeader, 16);
         memcpy(packet.m_pcData, b->m_pUnit->m_Packet.m_pcData, b->m_pUnit->m_Packet.getLength());

         packet.setLength(b->m_pUnit->m_Packet.getLength());

         //b->m_pUnit->m_bValid = false;
         delete [] b->m_pUnit->m_Packet.m_pcData;
         delete b->m_pUnit;
         b->m_pUnit = NULL;

         return packet.getLength();
      }

      b = b->m_pNext;
   }

   packet.setLength(-1);

   return -1;
}

void CHash::setUnit(const int32_t& id, CUnit* unit)
{
   CGuard hashguard(m_ListLock);

   CBucket* b = m_pBucket[id % m_iHashSize];

   while (NULL != b)
   {
      if (id == b->m_iID)
      {
         // only one packet can be stored in the hash table entry, the following should be discarded
         if (NULL != b->m_pUnit)
            return;

         b->m_pUnit = new CUnit;
         b->m_pUnit->m_Packet.m_pcData = new char [unit->m_Packet.getLength()];
         memcpy(b->m_pUnit->m_Packet.m_nHeader, unit->m_Packet.m_nHeader, 16);
         memcpy(b->m_pUnit->m_Packet.m_pcData, unit->m_Packet.m_pcData, unit->m_Packet.getLength());
         b->m_pUnit->m_Packet.setLength(unit->m_Packet.getLength());

         return;
      }

      b = b->m_pNext;
   }
}

void CHash::insert(const int32_t& id, const CUDT* u)
{
   CGuard hashguard(m_ListLock);

   CBucket* b = m_pBucket[id % m_iHashSize];

   CBucket* n = new CBucket;
   n->m_iID = id;
   n->m_pUDT = (CUDT*)u;
   n->m_pUnit = NULL;
   n->m_pNext = b;

   m_pBucket[id % m_iHashSize] = n;
}

void CHash::remove(const int32_t& id)
{
   CGuard hashguard(m_ListLock);

   CBucket* b = m_pBucket[id % m_iHashSize];

   if (NULL == b)
      return;

   if (id == b->m_iID)
   {
      m_pBucket[id % m_iHashSize] = b->m_pNext;
      delete b;

      return;
   }

   while (NULL != b->m_pNext)
   {
      if (id == b->m_pNext->m_iID)
      {
         CBucket* n = b->m_pNext;
         b->m_pNext = n->m_pNext;
         delete n;

         return;
      }

      b = b->m_pNext;
   }
}


//
CRcvQueue::CRcvQueue():
m_iQueueLen(0),
m_pActiveQueue(NULL),
m_iAQHeadPtr(0),
m_iAQTailPtr(0),
m_pPassiveQueue(NULL),
m_iPQHeadPtr(0),
m_iPQTailPtr(0),
m_pRcvUList(NULL),
m_pHash(NULL),
m_pChannel(NULL),
m_pTimer(NULL),
m_ListenerID(-1)
{
   #ifndef WIN32
      pthread_cond_init(&m_QueueCond, NULL);
      pthread_mutex_init(&m_QueueLock, NULL);
   #else
      m_QueueLock = CreateMutex(NULL, false, NULL);
      m_QueueCond = CreateEvent(NULL, false, false, NULL);
   #endif
}

CRcvQueue::~CRcvQueue()
{
   if (NULL != m_pActiveQueue)
      delete [] m_pActiveQueue;

   if (NULL != m_pPassiveQueue)
      delete [] m_pPassiveQueue;

   #ifndef WIN32
      pthread_cond_destroy(&m_QueueCond);
      pthread_mutex_destroy(&m_QueueLock);
   #else
      CloseHandle(m_QueueLock);
      CloseHandle(m_QueueCond);
   #endif
}

void CRcvQueue::init(const int& qsize, const int& payload, const int& hsize, const CChannel* cc, const CTimer* t)
{
   m_iQueueLen = qsize;
   m_iPayloadSize = payload;

   m_UnitQueue.init(qsize, payload);

   m_pActiveQueue = new CUnit*[qsize];
   m_pPassiveQueue = new CUnit*[qsize];

   m_pHash = new CHash;
   m_pHash->init(hsize);

   m_pChannel = (CChannel*)cc;
   m_pTimer = (CTimer*)t;

   m_pRcvUList = new CRcvUList;

   #ifndef WIN32
      pthread_create(&m_enQThread, NULL, CRcvQueue::enQueue, this);
      pthread_detach(m_enQThread);
      pthread_create(&m_deQThread, NULL, CRcvQueue::deQueue, this);
      pthread_detach(m_deQThread);
   #else
      DWORD threadID;
      m_enQThread = CreateThread(NULL, 0, CRcvQueue::enQueue, this, 0, &threadID);
      m_deQThread = CreateThread(NULL, 0, CRcvQueue::deQueue, this, 0, &threadID);
   #endif
}

#ifndef WIN32
   void* CRcvQueue::enQueue(void* param)
#else
   DWORD WINAPI CRcvQueue::enQueue(LPVOID param)
#endif
{
   CRcvQueue* self = (CRcvQueue*)param;

   bool empty = false;
   CUnit temp;
   temp.m_Packet.m_pcData = new char[self->m_iPayloadSize];
   CUnit* unit;

   while (true)
   {
      #ifdef NO_BUSY_WAITING
         self->m_pTimer->tick();
      #endif

      // find next available slot for incoming packet
      unit = self->m_UnitQueue.getNextAvailUnit();
      if (NULL == unit)
         unit = &temp;
      
      unit->m_Packet.setLength(self->m_iPayloadSize);

      // reading next incoming packet
      if (self->m_pChannel->recvfrom(unit->m_pAddr, unit->m_Packet) <= 0)
         continue;
      if (unit == &temp)
         continue;

      if ((self->m_iAQTailPtr == self->m_iAQHeadPtr) && (self->m_iPQTailPtr == self->m_iPQHeadPtr))
         empty = true;

      if (0 == unit->m_Packet.getFlag())
      {
         // queue is full, disgard the packet
         if ((self->m_iAQTailPtr + 1 == self->m_iAQHeadPtr) || ((self->m_iAQTailPtr == self->m_iQueueLen - 1) && (self->m_iAQHeadPtr == 0)))
            continue;

         self->m_UnitQueue.m_iCount ++;
         unit->m_bValid = true;

         // this is a data packet, put it into active queue
         self->m_pActiveQueue[self->m_iAQTailPtr] = unit;

         if (self->m_iQueueLen != self->m_iAQTailPtr + 1)
            ++ self->m_iAQTailPtr;
         else
            self->m_iAQTailPtr = 0;
      }
      else
      {
         // queue is full, disgard the packet
         if ((self->m_iPQTailPtr + 1 == self->m_iPQHeadPtr) || ((self->m_iPQTailPtr == self->m_iQueueLen - 1) && (self->m_iPQHeadPtr == 0)))
            continue;

         self->m_UnitQueue.m_iCount ++;
         unit->m_bValid = true;

         // this is a control packet, put it into passive queue
         self->m_pPassiveQueue[self->m_iPQTailPtr] = unit;

         if (self->m_iQueueLen != self->m_iPQTailPtr + 1)
            ++ self->m_iPQTailPtr;
         else
            self->m_iPQTailPtr = 0;
      }

      if (empty)
      {
         #ifndef WIN32
            pthread_cond_signal(&self->m_QueueCond);
         #else
            SetEvent(self->m_QueueCond);
         #endif

         empty = false;
      }
   }

   delete [] temp.m_Packet.m_pcData;

   return NULL;
}

#ifndef WIN32
   void* CRcvQueue::deQueue(void* param)
#else
   DWORD WINAPI CRcvQueue::deQueue(LPVOID param)
#endif
{
   CRcvQueue* self = (CRcvQueue*)param;

   while (true)
   {
      if (self->m_iPQTailPtr != self->m_iPQHeadPtr)
      {
         // check passive queue first, which has higher priority

         int32_t id = self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_Packet.m_iID;

         // ID 0 is for connection request, which should be passed to the listening socket or rendezvous sockets
         if (0 == id)
         {
            if (-1 != self->m_ListenerID)
               id = self->m_ListenerID;
            else if (0 != self->m_vRendezvousID.size())
               for (vector<CRL>::iterator i = self->m_vRendezvousID.begin(); i != self->m_vRendezvousID.end(); ++ i)
                  if (CIPAddress::ipcmp(self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_pAddr, i->m_pPeerAddr /*IPversion*/))
                  {
                     id = i->m_iID;
                     break;
                  }
         }

         CUDT* u = self->m_pHash->lookup(id);

         if (NULL != u)
         {
            // process the control packet, pass the connection request to listening socket, or temporally store in hash table

            if (u->m_bConnected && !u->m_bBroken)
            {
               u->processCtrl(self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_Packet);
               u->checkTimers();
            }
            else if (u->m_bListening)
               u->listen(self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_pAddr, self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_Packet);
            else
               self->m_pHash->setUnit(id, self->m_pPassiveQueue[self->m_iPQHeadPtr]);

            self->m_pRcvUList->remove(id);
            if (u->m_bConnected && !u->m_bBroken)
               self->m_pRcvUList->insert(id, u);
         }

         self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_bValid = false;
         self->m_UnitQueue.m_iCount --;

         if (self->m_iQueueLen != self->m_iPQHeadPtr + 1)
            ++ self->m_iPQHeadPtr;
         else
            self->m_iPQHeadPtr = 0;
      }
      else if (self->m_iAQTailPtr != self->m_iAQHeadPtr)
      {
         int32_t id = self->m_pActiveQueue[self->m_iAQHeadPtr]->m_Packet.m_iID;

         CUDT* u = self->m_pHash->lookup(id);

         if (NULL != u)
         {
            if (u->m_bConnected && !u->m_bBroken)
            {
               if (0 != u->processData(self->m_pActiveQueue[self->m_iAQHeadPtr]))
               {
                  self->m_pActiveQueue[self->m_iAQHeadPtr]->m_bValid = false;
                  self->m_UnitQueue.m_iCount --;
               }
               u->checkTimers();
            }

            self->m_pRcvUList->remove(id);
            if (u->m_bConnected && !u->m_bBroken)
               self->m_pRcvUList->insert(id, u);
         }
         else
         {
            self->m_pActiveQueue[self->m_iAQHeadPtr]->m_bValid = false;
            self->m_UnitQueue.m_iCount --;
         }

         if (self->m_iQueueLen != self->m_iAQHeadPtr + 1)
            ++ self->m_iAQHeadPtr;
         else
            self->m_iAQHeadPtr = 0;
      }
      else
      {
         // wait for a new packet
         #ifndef WIN32
            timespec timeout;
            uint64_t now = CTimer::getTime() + 10000;

            timeout.tv_sec = now / 1000000;
            timeout.tv_nsec = (now % 1000000) * 1000;

            if (0 == pthread_cond_timedwait(&self->m_QueueCond, &self->m_QueueLock, &timeout))
               continue;
         #else
            WaitForSingleObject(self->m_QueueCond, 1);
         #endif
      }

      // take care of the timing event for all UDT sockets

      CUDTList* ul = self->m_pRcvUList->m_pUList;
      uint64_t currtime;
      CTimer::rdtsc(currtime);

      while ((NULL != ul) && (ul->m_llTimeStamp < currtime - 10000 * CTimer::getCPUFrequency()))
      {
         CUDT* u = ul->m_pUDT;
         int32_t id = ul->m_iID;

         CPacket packet;
         packet.setLength(0);

         if (u->m_bConnected && !u->m_bBroken)
         {
            u->checkTimers();

            self->m_pRcvUList->remove(id);
            self->m_pRcvUList->insert(id, u);
         }
         else
         {
            self->m_pRcvUList->remove(id);
         }

         ul = self->m_pRcvUList->m_pUList;
      }
   }

   return NULL;
}

int CRcvQueue::recvfrom(sockaddr* , CPacket& packet, const int32_t& id)
{
   // read a packet from the temporay strorage in hash table

   int res;

   if ((res = m_pHash->retrieve(id, packet)) < 0)
   {
      #ifndef WIN32
         uint64_t now = CTimer::getTime();
         timespec timeout;

         timeout.tv_sec = now / 1000000 + 1;
         timeout.tv_nsec = now % 1000000 * 1000;

         pthread_cond_timedwait(&m_PassCond, &m_PassLock, &timeout);
      #else
         WaitForSingleObject(m_PassCond, 1);
      #endif
   }
   else
      return res;

   res = m_pHash->retrieve(id, packet);

   return res;
}

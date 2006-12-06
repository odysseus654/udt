/*****************************************************************************
Copyright � 2001 - 2006, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) special version UDT-m

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
This file contains the implementation of UDT multiplexer.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 12/05/2006
*****************************************************************************/

#include "common.h"
#include "queue.h"
#include "core.h"
#include <iostream>

using namespace std;

CSndUList::CSndUList():
m_pUList(NULL),
m_pLast(NULL)
{
   pthread_mutex_init(&m_ListLock, NULL);
}

CSndUList::~CSndUList()
{
   pthread_mutex_destroy(&m_ListLock);
}

void CSndUList::init()
{
   m_pUList = NULL;
   m_pLast = NULL;
}

void CSndUList::insert(const int64_t& ts, const int32_t& id, const CUDT* u)
{
   CGuard listguard(m_ListLock);

   if (NULL == m_pUList)
   {
      CUDTList* n = new CUDTList;
      CTimer::rdtsc(n->m_llTimeStamp);
      n->m_iID = id;
      n->m_pUDT = (CUDT*)u;

      n->m_pPrev = n->m_pNext = NULL;

      m_pLast = m_pUList = n;

      pthread_mutex_lock(m_pWindowLock);
      pthread_cond_signal(m_pWindowCond);
      pthread_mutex_unlock(m_pWindowLock);

      return;
   }

   CUDTList* n = new CUDTList;
   CTimer::rdtsc(n->m_llTimeStamp);
   n->m_iID = id;
   n->m_pUDT = (CUDT*)u;

   if (n->m_llTimeStamp > m_pLast->m_llTimeStamp)
   {
      n->m_pPrev = m_pLast;
      n->m_pNext = NULL;
      m_pLast->m_pNext = n;
      m_pLast = n;

      return;
   }

   CUDTList* p = m_pLast;
   while ((NULL != p) && (p->m_llTimeStamp > n->m_llTimeStamp))
      p = p->m_pPrev;

   if (NULL == p)
   {
      n->m_pPrev = NULL;
      n->m_pNext = m_pUList;
      m_pUList = n;

      return;
   }

   n->m_pPrev = p;
   n->m_pNext = p->m_pNext;
   p->m_pNext = n;
}

void CSndUList::remove(const int32_t& id)
{
   CGuard listguard(m_ListLock);

   if (NULL == m_pUList)
      return;

   if (id == m_pUList->m_iID)
   {
      CUDTList* n = m_pUList;
      m_pUList = m_pUList->m_pNext;
      if (m_pLast == n)
         m_pLast = NULL;
      else
         n->m_pNext->m_pPrev = NULL;
      delete n;
      return;
   }

   CUDTList* p = m_pUList->m_pNext;
   while (NULL != p)
   {
      if (id == p->m_iID)
      {
         p->m_pPrev->m_pNext = p->m_pNext;
         if (NULL != p->m_pNext)
            p->m_pNext->m_pPrev = p->m_pPrev;
         delete p;
      }

      p = p->m_pNext;
   }
}


//
CSndQueue::CSndQueue():
m_pUnitQueue(NULL),
m_iQueueLen(0),
m_iHeadPtr(0),
m_iTailPtr(0),
m_pPassiveQueue(NULL),
m_iPQHeadPtr(0),
m_iPQTailPtr(0),
m_pSndUList(NULL),
m_pChannel(NULL),
m_pTimer(NULL)
{
   pthread_cond_init(&m_QueueCond, NULL);
   pthread_mutex_init(&m_QueueLock, NULL);

   pthread_cond_init(&m_WindowCond, NULL);
   pthread_mutex_init(&m_WindowLock, NULL);
}

CSndQueue::~CSndQueue()
{
   pthread_cond_destroy(&m_QueueCond);
   pthread_mutex_destroy(&m_QueueLock);

   pthread_cond_destroy(&m_WindowCond);
   pthread_mutex_destroy(&m_WindowLock);

   if (NULL == m_pUnitQueue)
      return;

   delete [] m_pUnitQueue;
   m_pUnitQueue = NULL;

   delete m_pTimer;
}

void CSndQueue::init(const int& size, const CChannel* cc)
{
   m_iQueueLen = size;

   m_pUnitQueue = new CUnit[size];

   m_pPassiveQueue = new CUnit[size];

   m_pChannel = (CChannel*)cc;

   m_pTimer = new CTimer;

   m_pSndUList = new CSndUList;
   m_pSndUList->m_pWindowLock = &m_WindowLock;
   m_pSndUList->m_pWindowCond = &m_WindowCond;

   pthread_create(&m_enQThread, NULL, CSndQueue::enQueue, this);
   pthread_detach(m_enQThread);
   pthread_create(&m_deQThread, NULL, CSndQueue::deQueue, this);
   pthread_detach(m_deQThread);
}

void* CSndQueue::enQueue(void* param)
{
   CSndQueue* self = (CSndQueue*)param;

   while (true)
   {
      if (NULL != self->m_pSndUList->m_pUList)
      {
         uint64_t currtime;
         CTimer::rdtsc(currtime);

         if (currtime < self->m_pSndUList->m_pUList->m_llTimeStamp)
            self->m_pTimer->sleepto(self->m_pSndUList->m_pUList->m_llTimeStamp);

         uint64_t ts;

//cout << "sending " << self->m_pSndUList->m_pUList->m_pUDT->m_SocketID << " " << self->m_pSndUList->m_pUList->m_pUDT->m_bConnected << endl;

         int ps = self->m_pSndUList->m_pUList->m_pUDT->pack(self->m_pUnitQueue[self->m_iTailPtr].m_Packet, ts);

         bool empty = (self->m_iHeadPtr == self->m_iTailPtr);

         if (ps > 0)
         {
            self->m_pUnitQueue[self->m_iTailPtr].m_pAddr = self->m_pSndUList->m_pUList->m_pUDT->m_pPeerAddr;

            ++ self->m_iTailPtr;
            if (self->m_iQueueLen == self->m_iTailPtr)
               self->m_iTailPtr = 0;

            if (empty)
            {
               pthread_mutex_lock(&self->m_QueueLock);
               pthread_cond_signal(&self->m_QueueCond);
               pthread_mutex_unlock(&self->m_QueueLock);
            }
         }

         int32_t id = self->m_pSndUList->m_pUList->m_iID;
         CUDT* u = self->m_pSndUList->m_pUList->m_pUDT;

         self->m_pSndUList->remove(id);
         if (ts > 0)
            self->m_pSndUList->insert(ts, id, u);
      }
      else
      {
         pthread_mutex_lock(&self->m_WindowLock);
         if (NULL == self->m_pSndUList)
            pthread_cond_wait(&self->m_WindowCond, &self->m_WindowLock);
         pthread_mutex_unlock(&self->m_WindowLock);
      }
   }

   return NULL;
}

void* CSndQueue::deQueue(void* param)
{
   CSndQueue* self = (CSndQueue*)param;

   while (true)
   {
//cout << self->m_iHeadPtr << " " << self->m_iTailPtr << endl;

      if (self->m_iPQHeadPtr != self->m_iPQTailPtr)
      {
         self->m_pChannel->sendto(self->m_pPassiveQueue[self->m_iPQHeadPtr].m_pAddr, self->m_pPassiveQueue[self->m_iPQHeadPtr].m_Packet);

         ++ self->m_iPQHeadPtr;
         if (self->m_iQueueLen == self->m_iPQHeadPtr)
            self->m_iPQHeadPtr = 0;
      }
      else if (self->m_iHeadPtr != self->m_iTailPtr)
      {
         self->m_pChannel->sendto(self->m_pUnitQueue[self->m_iHeadPtr].m_pAddr, self->m_pUnitQueue[self->m_iHeadPtr].m_Packet);

         ++ self->m_iHeadPtr;
         if (self->m_iQueueLen == self->m_iHeadPtr)
            self->m_iHeadPtr = 0;
      }
      else
      {
         pthread_mutex_lock(&self->m_QueueLock);
         if ((self->m_iPQHeadPtr == self->m_iPQTailPtr) && (self->m_iHeadPtr == self->m_iTailPtr))
            pthread_cond_wait(&self->m_QueueCond, &self->m_QueueLock);
         pthread_mutex_unlock(&self->m_QueueLock);
      }
   }

   return NULL;
}

int CSndQueue::sendto(const sockaddr* addr, const CPacket& packet)
{
   m_pChannel->sendto(addr, packet);

//cout << "sndqueue sendto " << packet.getLength() << " " << packet.getType() << endl;

   return packet.getLength();
}


//
void CRcvUList::init()
{
   m_pUList = NULL;
   m_pLast = NULL;
}

void CRcvUList::insert(const int32_t& id, const CUDT* u)
{
//   cout << "RCVULIST insert " << id << " " << long(u) << endl;

   if (NULL == m_pUList)
   {
      m_pUList = new CUDTList;
      CTimer::rdtsc(m_pUList->m_llTimeStamp);
      m_pUList->m_iID = id;
      m_pUList->m_pUDT = (CUDT*)u;

      m_pUList->m_pPrev = m_pUList->m_pNext = NULL;

      m_pLast = m_pUList;

      return;
   }

   CUDTList* n = new CUDTList;
   CTimer::rdtsc(n->m_llTimeStamp);
   n->m_iID = id;
   n->m_pUDT = (CUDT*)u;
   n->m_pPrev = m_pLast;
   n->m_pNext = NULL;
   m_pLast->m_pNext = n;
   m_pLast = n;
}

void CRcvUList::remove(const int32_t& id)
{
//cout << "RCVULIST remove " << id << endl;

   if (NULL == m_pUList)
      return;

   if (id == m_pUList->m_iID)
   {
      CUDTList* n = m_pUList;
      m_pUList = m_pUList->m_pNext;
      if (m_pLast == n)
         m_pLast = NULL;
      else
         m_pUList->m_pPrev = NULL;
      delete n;
      return;
   }

   CUDTList* p = m_pUList;
   while (NULL != p->m_pNext)
   {
      if (id == p->m_pNext->m_iID)
      {
         CUDTList* n = p->m_pNext;
         p->m_pNext = n->m_pNext;
         if (NULL != n->m_pNext)
            n->m_pNext->m_pPrev = p;
         if (m_pLast == n)
            m_pLast = p;
         delete n;
         return;
      }

      p = p->m_pNext;
   }
}


//
void CHash::init(const int& size)
{
   m_pBucket = new (CBucket*)[size];

   for (int i = 0; i < size; ++ i)
      m_pBucket[i] = NULL;

   m_iHashSize = size;
}

CUDT* CHash::lookup(const int32_t& id)
{
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
   CBucket* b = m_pBucket[id % m_iHashSize];

   while (NULL != b)
   {
      if ((id == b->m_iID) && (NULL != b->m_pUnit))
      {
         memcpy(packet.m_nHeader, b->m_pUnit->m_Packet.m_nHeader, 16);
         memcpy(packet.m_pcData, b->m_pUnit->m_Packet.m_pcData, b->m_pUnit->m_Packet.getLength());

//int* p = (int*)(b->m_pUnit->m_Packet.m_pcData);
//cout << "RETRIEVE UNIT " << p[0] << " " << p[1] << " " << p[2] << " " << p[3] << endl;

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
   CBucket* b = m_pBucket[id % m_iHashSize];

   while (NULL != b)
   {
      if (id == b->m_iID)
      {
         if (NULL != b->m_pUnit)
         {
            return;
            //delete [] b->m_pUnit->m_Packet.m_pcData;
            //delete b->m_pUnit;
         }

         b->m_pUnit = new CUnit;
         b->m_pUnit->m_Packet.m_pcData = new char [unit->m_Packet.getLength()];
         memcpy(b->m_pUnit->m_Packet.m_nHeader, unit->m_Packet.m_nHeader, 16);
         memcpy(b->m_pUnit->m_Packet.m_pcData, unit->m_Packet.m_pcData, unit->m_Packet.getLength());
         b->m_pUnit->m_Packet.setLength(unit->m_Packet.getLength());

//int* p = (int*)(unit->m_Packet.m_pcData);
//cout << "SET UNIT " << p[0] << " " << p[1] << " " << p[2] << " " << p[3] << endl;

         return;
      }

      b = b->m_pNext;
   }
}

void CHash::insert(const int32_t& id, const CUDT* u)
{
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
   CBucket* b = m_pBucket[id % m_iHashSize];

   if (NULL == b)
      return;

   if (id == b->m_iID)
   {
      CBucket* n = b;
      b = b->m_pNext;
      delete n;

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
m_pUnitQueue(NULL),
m_iQueueLen(0),
m_iUnitSize(0),
m_iPtr(0),
m_pActiveQueue(NULL),
m_iAQHeadPtr(0),
m_iAQTailPtr(0),
m_pPassiveQueue(NULL),
m_iPQHeadPtr(0),
m_iPQTailPtr(0),
m_pRcvUList(NULL),
m_pHash(NULL),
m_pChannel(NULL),
m_ListenerID(-1)
{
   pthread_cond_init(&m_QueueCond, NULL);
   pthread_mutex_init(&m_QueueLock, NULL);
}

CRcvQueue::~CRcvQueue()
{
   if (NULL != m_pUnitQueue)
   {
      for (int i = 0; i < m_iQueueLen; ++ i)
         delete [] m_pUnitQueue[i].m_Packet.m_pcData;

      delete [] m_pUnitQueue;
      m_pUnitQueue = NULL;
   }

   if (NULL != m_pActiveQueue)
      delete [] m_pActiveQueue;

   if (NULL != m_pPassiveQueue)
      delete [] m_pPassiveQueue;

   pthread_cond_destroy(&m_QueueCond);
   pthread_mutex_destroy(&m_QueueLock);
}

void CRcvQueue::init(const int& qsize, const int& mss, const int& hsize, const CChannel* cc)
{
//cout << "initilize " << qsize << " " << mss << " " << hsize << endl;

   m_iQueueLen = qsize;

   m_pUnitQueue = new CUnit[qsize];
   for (int i = 0; i < m_iQueueLen; ++ i)
   {
      m_pUnitQueue[i].m_bValid = false;
      m_pUnitQueue[i].m_Packet.m_pcData = new char[mss];
      m_pUnitQueue[i].m_pAddr = (sockaddr*)new sockaddr_in;
   }

   m_pActiveQueue = new CUnit*[qsize];
   m_pPassiveQueue = new CUnit*[qsize];

   m_pHash = new CHash;
   m_pHash->init(hsize);

   m_pChannel = (CChannel*)cc;

   m_pRcvUList = new CRcvUList;

   //cout << "INIT -------------------" << long(this) << " " << long(m_pChannel) << endl;

   pthread_create(&m_enQThread, NULL, CRcvQueue::enQueue, this);
   pthread_detach(m_enQThread);
   pthread_create(&m_deQThread, NULL, CRcvQueue::deQueue, this);
   pthread_detach(m_deQThread);

   //cout << "INIT ++++++++++++++++++" << long(this) << " " << long(m_pChannel) << endl;
}

void* CRcvQueue::enQueue(void* param)
{
   CRcvQueue* self = (CRcvQueue*)param;

   bool empty = false;

   while (true)
   {
      while (self->m_pUnitQueue[self->m_iPtr].m_bValid)
      {
         ++ self->m_iPtr;

         if (self->m_iPtr == self->m_iQueueLen)
            self->m_iPtr = 0;
      }

      self->m_pUnitQueue[self->m_iPtr].m_Packet.setLength(1460);

//cout << "reading packet in CRcvQueue::enQueue " << self->m_iPtr << endl;
      if (self->m_pChannel->recvfrom(self->m_pUnitQueue[self->m_iPtr].m_pAddr, self->m_pUnitQueue[self->m_iPtr].m_Packet) <= 0)
         continue;


//cout << "recv " << self->m_pUnitQueue[self->m_iPtr].m_Packet.getType() << " " << self->m_pUnitQueue[self->m_iPtr].m_Packet.getLength() << " " << self->m_pUnitQueue[self->m_iPtr].m_Packet.getFlag() << endl;

      if ((self->m_iAQTailPtr == self->m_iAQHeadPtr) && (self->m_iPQTailPtr == self->m_iPQHeadPtr))
         empty = true;

      if (0 == self->m_pUnitQueue[self->m_iPtr].m_Packet.getFlag())
      {
         // queue is full, disgard the packet
         if ((self->m_iAQTailPtr + 1 == self->m_iAQHeadPtr) || ((self->m_iAQTailPtr == self->m_iQueueLen) && (self->m_iAQHeadPtr == 0)))
            continue;

         self->m_pActiveQueue[self->m_iAQTailPtr] = self->m_pUnitQueue + self->m_iPtr;
         ++ self->m_iAQTailPtr;
         if (self->m_iQueueLen == self->m_iAQTailPtr)
            self->m_iAQTailPtr = 0;

//cout << "AQ = " << self->m_iAQTailPtr << endl;
      }
      else
      {
         // queue is full, disgard the packet
         if ((self->m_iPQTailPtr + 1 == self->m_iPQHeadPtr) || ((self->m_iPQTailPtr == self->m_iQueueLen) && (self->m_iPQHeadPtr == 0)))
            continue;

         self->m_pPassiveQueue[self->m_iPQTailPtr] = self->m_pUnitQueue + self->m_iPtr;
         ++ self->m_iPQTailPtr;
         if (self->m_iQueueLen == self->m_iPQTailPtr)
            self->m_iPQTailPtr = 0;
      }

      self->m_pUnitQueue[self->m_iPtr].m_bValid = true;

      if (empty)
      {
         pthread_cond_signal(&self->m_QueueCond);
         empty = false;
      }
   }

   return NULL;
}

void* CRcvQueue::deQueue(void* param)
{
   CRcvQueue* self = (CRcvQueue*)param;

   while (true)
   {
      if (self->m_iPQTailPtr != self->m_iPQHeadPtr)
      {
         int32_t id = self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_Packet.m_iID;

         if ((0 == id) && (-1 != self->m_ListenerID))
            id = self->m_ListenerID;

//cout << "IDDDDDD: " << id << endl;

         CUDT* u = self->m_pHash->lookup(id);
/*
if (NULL == u)
   cout << "NULL u \n";
else
   cout << "??????? " << u->m_SocketID << " " << u->m_bConnected << " " << u->m_bListening << endl;
*/
         if (NULL != u)
         {
            if (u->m_bConnected)
               u->process(self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_Packet);
            else if (u->m_bListening)
            {
//cout << "processing connection request~\n";
               u->listen(self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_pAddr, self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_Packet);
            }
            else
            {
               self->m_pHash->setUnit(id, self->m_pPassiveQueue[self->m_iPQHeadPtr]);
            }

            self->m_pRcvUList->remove(id);
            self->m_pRcvUList->insert(id, u);
         }

         self->m_pPassiveQueue[self->m_iPQHeadPtr]->m_bValid = false;

         ++ self->m_iPQHeadPtr;
         if (self->m_iQueueLen == self->m_iPQHeadPtr)
            self->m_iPQHeadPtr = 0;
      }
      else if (self->m_iAQTailPtr != self->m_iAQHeadPtr)
      {
         int32_t id = self->m_pActiveQueue[self->m_iAQHeadPtr]->m_Packet.m_iID;

//cout << "RCV AQ " << id << endl;

         CUDT* u = self->m_pHash->lookup(id);

         if (NULL != u)
         {
            u->process(self->m_pActiveQueue[self->m_iAQHeadPtr]->m_Packet);

            self->m_pRcvUList->remove(id);
            self->m_pRcvUList->insert(id, u);
         }
//         else
//            cout << "U ++ NULL!!!\n";

         self->m_pActiveQueue[self->m_iAQHeadPtr]->m_bValid = false;

         ++ self->m_iAQHeadPtr;
         if (self->m_iQueueLen == self->m_iAQHeadPtr)
            self->m_iAQHeadPtr = 0;
      }
      else
      {
         timespec timeout;
         timeval now;

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

         if (0 == pthread_cond_timedwait(&self->m_QueueCond, &self->m_QueueLock, &timeout))
            continue;
      }

      CUDTList* ul = self->m_pRcvUList->m_pUList;

      uint64_t currtime;
      CTimer::rdtsc(currtime);
      while ((NULL != ul) && (ul->m_llTimeStamp < currtime - 10000 * CTimer::getCPUFrequency()))
      {
         CUDT* u = ul->m_pUDT;
         int32_t id = ul->m_iID;

         CPacket packet;
         packet.setLength(0);

         if (u->m_bConnected)
            u->process(packet);

         self->m_pRcvUList->remove(id);
         self->m_pRcvUList->insert(id, u);

         ul = self->m_pRcvUList->m_pUList;
      }
   }

   return NULL;
}

int CRcvQueue::recvfrom(sockaddr* addr, CPacket& packet, const int32_t& id)
{
   timeval now;
   timespec timeout;
   gettimeofday(&now, 0);
   timeout.tv_sec = now.tv_sec + 1;
   timeout.tv_nsec = now.tv_usec * 1000;

   if (m_pHash->retrieve(id, packet) < 0)
      pthread_cond_timedwait(&m_PassCond, &m_PassLock, &timeout);

   int res = m_pHash->retrieve(id, packet);

//int* p = (int*)(packet.m_pcData);
//cout << "READ it " << packet.getLength() << " " << p[0] << " " << p[1] << " " << p[2] << " " << p[3] << endl;

   return res;
}

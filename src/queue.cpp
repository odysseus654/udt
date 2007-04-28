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
   Yunhong Gu [gu@lac.uic.edu], last updated 04/27/2007
*****************************************************************************/

#ifdef WIN32
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif

#include "common.h"
#include "queue.h"
#include "core.h"


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

   for (vector<char*>::iterator j = m_vpBuffer.begin(); j != m_vpBuffer.end(); ++ j)
      delete [] *j;
   m_vpBuffer.clear();

   for (vector<char*>::iterator k = m_vpAddrBuf.begin(); k != m_vpAddrBuf.end(); ++ k)
      delete [] *k;
   m_vpAddrBuf.clear();

   m_viSize.clear();
}

int CUnitQueue::init(const int& size, const int& mss, const int& version)
{
   CUnit* tempu = NULL;
   char* tempb = NULL;
   char* tempa = NULL;

   try
   {
      tempu = new CUnit [size];
      tempb = new char [size * mss];
      tempa = (AF_INET == version) ? (char*) new sockaddr_in [size] : (char*) new sockaddr_in6 [size];
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
      tempu[i].m_pAddr = (AF_INET == version) ? (sockaddr*)((sockaddr_in*)tempa + i) : (sockaddr*)((sockaddr_in6*)tempa + i);
      tempu[i].m_Packet.m_pcData = tempb + i * mss;
   }

   m_vpUnit.insert(m_vpUnit.end(), tempu);
   m_vpBuffer.insert(m_vpBuffer.end(), tempb);
   m_vpAddrBuf.insert(m_vpAddrBuf.end(), tempa);
   m_viSize.insert(m_viSize.end(), size);

   m_iSize = size;
   m_iMSS = mss;
   m_iIPversion = version;

   m_pAvailUnit = m_vpUnit[0];
   m_iVQ = 0;

   return 0;
}

int CUnitQueue::increase()
{
   // adjust/correct m_iCount
   int real_count = 0;
   for (unsigned int i = 0; i < m_vpUnit.size(); ++ i)
   {
      CUnit* p = m_vpUnit[i];
      for (CUnit* end = p + m_viSize[i] - 1; p != end; ++ p)
         if (p->m_bValid)
            ++ real_count;
   }
   m_iCount = real_count;
   if (double(m_iCount) / m_iSize < 0.9)
      return -1;

   CUnit* tempu = NULL;
   char* tempb = NULL;
   char* tempa = NULL;

   try
   {
      tempu = new CUnit [m_iSize];
      tempb = new char [m_iSize * m_iMSS];
      tempa = (AF_INET == m_iIPversion) ? (char*) new sockaddr_in [m_iSize] : (char*) new sockaddr_in6 [m_iSize];;
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
      tempu[i].m_pAddr = (AF_INET == m_iIPversion) ? (sockaddr*)((sockaddr_in*)tempa + i) : (sockaddr*)((sockaddr_in6*)tempa + i);
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
      for (CUnit* sentinel = m_vpUnit[m_iVQ] + m_viSize[m_iVQ] - 1; m_pAvailUnit != sentinel; ++ m_pAvailUnit)
         if (!m_pAvailUnit->m_bValid)
            return m_pAvailUnit;

      if (m_iVQ != int(m_vpUnit.size() - 1))
         ++ m_iVQ;
      else
         m_iVQ = 0;

      m_pAvailUnit = m_vpUnit[m_iVQ];
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
m_pSndUList(NULL),
m_pChannel(NULL),
m_pTimer(NULL)
{
   #ifndef WIN32
      pthread_cond_init(&m_WindowCond, NULL);
      pthread_mutex_init(&m_WindowLock, NULL);
   #else
      m_WindowLock = CreateMutex(NULL, false, NULL);
      m_WindowCond = CreateEvent(NULL, false, false, NULL);
   #endif
}

CSndQueue::~CSndQueue()
{
   #ifndef WIN32
      pthread_cond_destroy(&m_WindowCond);
      pthread_mutex_destroy(&m_WindowLock);
   #else
      CloseHandle(m_WindowLock);
      CloseHandle(m_WindowCond);
   #endif
}

void CSndQueue::init(const CChannel* c, const CTimer* t)
{
   m_pChannel = (CChannel*)c;
   m_pTimer = (CTimer*)t;
   m_pSndUList = new CSndUList;
   m_pSndUList->m_pWindowLock = &m_WindowLock;
   m_pSndUList->m_pWindowCond = &m_WindowCond;

   #ifndef WIN32
      pthread_create(&m_WorkerThread, NULL, CSndQueue::worker, this);
      pthread_detach(m_WorkerThread);
   #else
      DWORD threadID;
      m_WorkerThread = CreateThread(NULL, 0, CSndQueue::worker, this, 0, &threadID);
   #endif
}

#ifndef WIN32
   void* CSndQueue::worker(void* param)
#else
   DWORD WINAPI CSndQueue::worker(LPVOID param)
#endif
{
   CSndQueue* self = (CSndQueue*)param;

   CPacket pkt;

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

         // pack a packet from the socket
         uint64_t ts;
         if (u->packData(pkt, ts) > 0)
            self->m_pChannel->sendto(u->m_pPeerAddr, pkt);

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

   m_vNewEntry.clear();
}

CRcvUList::~CRcvUList()
{
   #ifndef WIN32
      pthread_mutex_destroy(&m_ListLock);
   #else
      CloseHandle(m_ListLock);
   #endif
}

void CRcvUList::insert(const CUDT* u)
{
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

void CRcvUList::newEntry(CUDT* u)
{
   CGuard listguard(m_ListLock);
   m_vNewEntry.insert(m_vNewEntry.end(), u);
}

bool CRcvUList::ifNewEntry()
{
   return m_vNewEntry.empty();
}

CUDT* CRcvUList::newEntry()
{
   CGuard listguard(m_ListLock);
   CUDT* u = (CUDT*)*(m_vNewEntry.begin());
   m_vNewEntry.erase(m_vNewEntry.begin());

   return u;
}

//
void CHash::init(const int& size)
{
   m_pBucket = new CBucket* [size];

   for (int i = 0; i < size; ++ i)
      m_pBucket[i] = NULL;

   m_iHashSize = size;
}

CUDT* CHash::lookup(const int32_t& id)
{
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
   CBucket* b = m_pBucket[id % m_iHashSize];

   while (NULL != b)
   {
      if (id == b->m_iID)
      {
         // only one packet can be stored in the hash table entry, the following should be discarded
         if (NULL != b->m_pUnit)
            return;

         CUnit* tmp = new CUnit;
         tmp->m_Packet.m_pcData = new char [unit->m_Packet.getLength()];
         memcpy(tmp->m_Packet.m_nHeader, unit->m_Packet.m_nHeader, 16);
         memcpy(tmp->m_Packet.m_pcData, unit->m_Packet.m_pcData, unit->m_Packet.getLength());
         tmp->m_Packet.setLength(unit->m_Packet.getLength());

         b->m_pUnit = tmp;

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
m_pRcvUList(NULL),
m_pHash(NULL),
m_pChannel(NULL),
m_pTimer(NULL),
m_ListenerID(-1)
{
   #ifndef WIN32
      pthread_cond_init(&m_PassCond, NULL);
      pthread_mutex_init(&m_PassLock, NULL);
   #else
      m_PassLock = CreateMutex(NULL, false, NULL);
      m_PassCond = CreateEvent(NULL, false, false, NULL);
   #endif
}

CRcvQueue::~CRcvQueue()
{
   #ifndef WIN32
      pthread_cond_destroy(&m_PassCond);
      pthread_mutex_destroy(&m_PassLock);
   #else
      CloseHandle(m_PassLock);
      CloseHandle(m_PassCond);
   #endif
}

void CRcvQueue::init(const int& qsize, const int& payload, const int& version, const int& hsize, const CChannel* cc, const CTimer* t)
{
   m_iPayloadSize = payload;

   m_UnitQueue.init(qsize, payload, version);

   m_pHash = new CHash;
   m_pHash->init(hsize);

   m_pChannel = (CChannel*)cc;
   m_pTimer = (CTimer*)t;

   m_pRcvUList = new CRcvUList;

   #ifndef WIN32
      pthread_create(&m_WorkerThread, NULL, CRcvQueue::worker, this);
      pthread_detach(m_WorkerThread);
   #else
      DWORD threadID;
      m_WorkerThread = CreateThread(NULL, 0, CRcvQueue::worker, this, 0, &threadID);
   #endif
}

#ifndef WIN32
   void* CRcvQueue::worker(void* param)
#else
   DWORD WINAPI CRcvQueue::worker(LPVOID param)
#endif
{
   CRcvQueue* self = (CRcvQueue*)param;

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

      CUDT* u;
      int32_t id;

      // reading next incoming packet
      if (self->m_pChannel->recvfrom(unit->m_pAddr, unit->m_Packet) <= 0)
         goto TIMER_CHECK;
      if (unit == &temp)
         goto TIMER_CHECK;

      id = unit->m_Packet.m_iID;

      // ID 0 is for connection request, which should be passed to the listening socket or rendezvous sockets
      if (0 == id)
      {
         if (-1 != self->m_ListenerID)
            id = self->m_ListenerID;
         else if (0 != self->m_vRendezvousID.size())
         {
            UDTSOCKET peerid = ((CHandShake*)unit->m_Packet.m_pcData)->m_iID;
            for (vector<CRL>::iterator i = self->m_vRendezvousID.begin(); i != self->m_vRendezvousID.end(); ++ i)
               if (CIPAddress::ipcmp(unit->m_pAddr, i->m_pPeerAddr, i->m_iIPversion) && ((0 == i->m_iPeerID) || (i->m_iPeerID == peerid)))
               {
                  id = i->m_iID;
                  i->m_iPeerID = peerid;
                  break;
               }
         }
      }

      u = self->m_pHash->lookup(id);

      if (NULL != u)
      {
         if (0 == unit->m_Packet.getFlag())
         {
            if (u->m_bConnected && !u->m_bBroken)
            {
               u->processData(unit);
               u->checkTimers();
            }
         }
         else
         {
            // process the control packet, pass the connection request to listening socket, or temporally store in hash table

            if (u->m_bConnected && !u->m_bBroken)
            {
               u->processCtrl(unit->m_Packet);
               u->checkTimers();
            }
            else if (u->m_bListening)
               u->listen(unit->m_pAddr, unit->m_Packet);
            else
            {
               self->m_pHash->setUnit(id, unit);

               #ifndef WIN32
                  pthread_cond_signal(&self->m_PassCond);
               #else
                  SetEvent(self->m_PassCond);
               #endif
            }
         }

         self->m_pRcvUList->remove(id);
         if (u->m_bConnected && !u->m_bBroken)
            self->m_pRcvUList->insert(u);
      }

TIMER_CHECK:
      // take care of the timing event for all UDT sockets

      // check waiting list, if new socket, insert it to the list
      if (!self->m_pRcvUList->ifNewEntry())
         self->m_pRcvUList->insert(self->m_pRcvUList->newEntry());

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
            self->m_pRcvUList->insert(u);
         }
         else
         {
            self->m_pRcvUList->remove(id);
            self->m_pHash->remove(id);
         }

         ul = self->m_pRcvUList->m_pUList;
      }
   }


   delete [] temp.m_Packet.m_pcData;

   return NULL;
}

int CRcvQueue::recvfrom(sockaddr* , CPacket& packet, const int32_t& id)
{
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

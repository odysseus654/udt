/*****************************************************************************
Copyright © 2001 - 2006, The Board of Trustees of the University of Illinois.
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
This header file contains the definition of UDT multiplexer.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 12/05/2006
*****************************************************************************/


#ifndef __UDT_QUEUE_H__
#define __UDT_QUEUE_H__

#include <pthread.h>
#include "packet.h"
#include "channel.h"

class CUDT;

struct CUnit
{
   sockaddr* m_pAddr;
   CPacket m_Packet;

   bool m_bValid;
};

struct CUDTList
{
   uint64_t m_llTimeStamp;
   int32_t m_iID;
   CUDT* m_pUDT;

   CUDTList* m_pPrev;
   CUDTList* m_pNext;
};

class CSndUList
{
friend class CSndQueue;

public:
   CSndUList();
   ~CSndUList();

   void init();
   void insert(const int64_t& ts, const int32_t& id, const CUDT* u);
   void remove(const int32_t& id);

   CUDTList* m_pUList;
   CUDTList* m_pLast;

private:
   pthread_mutex_t m_ListLock;

   pthread_mutex_t* m_pWindowLock;
   pthread_cond_t* m_pWindowCond;
};

struct CRcvUList
{
   void init();
   void insert(const int32_t& id, const CUDT* u);
   void remove(const int32_t& id);

   CUDTList* m_pUList;
   CUDTList* m_pLast;
};

struct CHash
{
   void init(const int& size);
   CUDT* lookup(const int32_t& id);
   int retrieve(const int32_t& id, CPacket& packet);
   void setUnit(const int32_t& id, CUnit* unit);
   void insert(const int32_t& id, const CUDT* u);
   void remove(const int32_t& id);

   struct CBucket
   {
      int32_t m_iID;
      CUDT* m_pUDT;

      CBucket* m_pNext;

      CUnit* m_pUnit;
   } **m_pBucket;

   int m_iHashSize;
};


class CSndQueue
{
friend class CUDT;

public:
   CSndQueue();
   ~CSndQueue();

public:
   void init(const int& size, const CChannel* cc);

public:
   static void* enQueue(void* param);
   static void* deQueue(void* param);

   int sendto(const sockaddr* addr, const CPacket& packet);

private:
   CUnit* m_pUnitQueue;
   int m_iQueueLen;

   volatile int m_iHeadPtr;
   volatile int m_iTailPtr;

   CUnit* m_pPassiveQueue;
   int m_iPQLen;

   volatile int m_iPQHeadPtr;
   volatile int m_iPQTailPtr;

private:
   CSndUList* m_pSndUList;

private:
   pthread_mutex_t m_QueueLock;
   pthread_cond_t m_QueueCond;

   pthread_mutex_t m_WindowLock;
   pthread_cond_t m_WindowCond;

private:
   pthread_t m_enQThread;
   pthread_t m_deQThread;

private:
   CChannel* m_pChannel;

   CTimer* m_pTimer;
};


class CRcvQueue
{
friend class CUDT;

public:
   CRcvQueue();
   ~CRcvQueue();

public:
   void init(const int& size, const int& mss, const int& hsize, const CChannel* cc);

   static void* enQueue(void* param);
   static void* deQueue(void* param);

   int recvfrom(sockaddr* addr, CPacket& packet, const int32_t& id);

private:
   CUnit* m_pUnitQueue;
   int m_iQueueLen;
   int m_iUnitSize;
   int m_iPtr;

   CUnit** m_pActiveQueue;
   int m_iAQHeadPtr;
   int m_iAQTailPtr;

   CUnit** m_pPassiveQueue;
   int m_iPQHeadPtr;
   int m_iPQTailPtr;

   pthread_mutex_t m_PassLock;
   pthread_cond_t m_PassCond;

private:
   CRcvUList* m_pRcvUList;

private:
   CHash* m_pHash;

private:
   pthread_t m_enQThread;
   pthread_t m_deQThread;

   pthread_cond_t m_QueueCond;
   pthread_mutex_t m_QueueLock;

private:
   CChannel* m_pChannel;

private:
   volatile UDTSOCKET m_ListenerID;
};


class CMultiplexer
{
public:
   CSndQueue* m_pSndQueue;
   CRcvQueue* m_pRcvQueue;
   CChannel* m_pChannel;

   int m_iPort;

   int m_iRefCount;
};

#endif

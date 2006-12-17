/*****************************************************************************
Copyright © 2001 - 2006, The Board of Trustees of the University of Illinois.
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
This header file contains the definition of UDT multiplexer.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 12/12/2006
*****************************************************************************/


#ifndef __UDT_QUEUE_H__
#define __UDT_QUEUE_H__

#include "common.h"
#include "packet.h"
#include "channel.h"


class CUDT;

struct CUnit
{
   sockaddr* m_pAddr;		// source address
   CPacket m_Packet;		// packet

   bool m_bValid;		// if this is a valid entry
};


struct CUDTList
{
   uint64_t m_llTimeStamp;	// Time Stamp
   int32_t m_iID;		// UDT Socket ID
   CUDT* m_pUDT;		// Pointer to the instance of CUDT socket

   CUDTList* m_pPrev;		// previous link
   CUDTList* m_pNext;		// next link
};


class CSndUList
{
friend class CSndQueue;

public:
   CSndUList();
   ~CSndUList();

public:

      // Functionality:
      //    Insert a new UDT instance into the list.
      // Parameters:
      //    1) [in] ts: time stamp: next processing time
      //    2) [in] id: socket ID
      //    3) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void insert(const int64_t& ts, const int32_t& id, const CUDT* u);

      // Functionality:
      //    Remove UDT instance from the list.
      // Parameters:
      //    1) [in] id: Socket ID
      // Returned value:
      //    None.

   void remove(const int32_t& id);

      // Functionality:
      //    Look for a specific UDT instance.
      // Parameters:
      //    1) [in] id: Socket ID
      // Returned value:
      //    True if found, otherwise false.

   bool find(const int32_t& id);

      // Functionality:
      //    Update the timestamp of the UDT instance on the list.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void update(const int32_t& id, const CUDT* u);

      // Functionality:
      //    Get and remove the first UDT instance on the list.
      // Parameters:
      //    1) [out] id: socket ID
      //    2) [out] u: pointer to the UDT instance
      // Returned value:
      //    UDT Socket ID if found one, otherwise -1.

   int pop(int32_t& id, CUDT*& u);

public:
   CUDTList* m_pUList;		// The head node

private:
   CUDTList* m_pLast;		// The last node

private:
   pthread_mutex_t m_ListLock;

   pthread_mutex_t* m_pWindowLock;
   pthread_cond_t* m_pWindowCond;
};


class CRcvUList
{
public:
   CRcvUList();
   ~CRcvUList();

public:

      // Functionality:
      //    Insert a new UDT instance to the list.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void insert(const int32_t& id, const CUDT* u);

      // Functionality:
      //    Remove the UDT instance to the list.
      // Parameters:
      //    1) [in] id: socket ID
      // Returned value:
      //    None.

   void remove(const int32_t& id);

public:
   CUDTList* m_pUList;

private:
   CUDTList* m_pLast;

private:
   pthread_mutex_t m_ListLock;
};

class CHash
{
public:
   CHash();
   ~CHash();

public:

      // Functionality:
      //    Initialize the hash table.
      // Parameters:
      //    1) [in] size: hash table size
      // Returned value:
      //    None.

   void init(const int& size);

      // Functionality:
      //    Look for a UDT instance from the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      // Returned value:
      //    Pointer to a UDT instance, or NULL if not found.

   CUDT* lookup(const int32_t& id);

      // Functionality:
      //    Retrive a received packet that is temporally stored in the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [out] packet: the returned packet
      // Returned value:
      //    Data length of the packet, or -1.

   int retrieve(const int32_t& id, CPacket& packet);

      // Functionality:
      //    Store a packet in the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [in] unit: information for the packet
      // Returned value:
      //    None.

   void setUnit(const int32_t& id, CUnit* unit);

      // Functionality:
      //    Insert an entry to the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      //    2) [in] u: pointer to the UDT instance
      // Returned value:
      //    None.

   void insert(const int32_t& id, const CUDT* u);

      // Functionality:
      //    Remove an entry from the hash table.
      // Parameters:
      //    1) [in] id: socket ID
      // Returned value:
      //    None.

   void remove(const int32_t& id);

private:
   struct CBucket
   {
      int32_t m_iID;		// Socket ID
      CUDT* m_pUDT;		// Socket instance

      CBucket* m_pNext;		// next bucket

      CUnit* m_pUnit;		// tempory buffer for a received packet
   } **m_pBucket;		// list of buckets (the hash table)

   int m_iHashSize;		// size of hash table

private:
   pthread_mutex_t m_ListLock;
};


class CSndQueue
{
friend class CUDT;
friend class CUDTUnited;

public:
   CSndQueue();
   ~CSndQueue();

public:

      // Functionality:
      //    Initialize the sending queue.
      // Parameters:
      //    1) [in] size: queue size
      //    2) [in] c: UDP channel to be associated to the queue
      //    3) [in] t: Timer
      // Returned value:
      //    None.

   void init(const int& size, const CChannel* c, const CTimer* t);

      // Functionality:
      //    Send out a packet to a given address.
      // Parameters:
      //    1) [in] addr: destination address
      //    2) [in] packet: packet to be sent out
      // Returned value:
      //    Size of data sent out.

   int sendto(const sockaddr* addr, CPacket& packet);

private:
#ifndef WIN32
   static void* enQueue(void* param);
   static void* deQueue(void* param);
#else
   static DWORD WINAPI enQueue(LPVOID param);
   static DWORD WINAPI deQueue(LPVOID param);
#endif

   pthread_t m_enQThread;
   pthread_t m_deQThread;

private:
   CUnit* m_pUnitQueue;			// The queue
   int m_iQueueLen;			// Length of the queue

   volatile int m_iHeadPtr;		// Head pointer of the queue
   volatile int m_iTailPtr;		// Tail pointer of the queue

private:
   CSndUList* m_pSndUList;		// List of UDT instances for data sending
   CChannel* m_pChannel;                // The UDP channel for data sending
   CTimer* m_pTimer;			// Timing facility

private:
   pthread_mutex_t m_QueueLock;
   pthread_cond_t m_QueueCond;

   pthread_mutex_t m_WindowLock;
   pthread_cond_t m_WindowCond;
};


class CRcvQueue
{
friend class CUDT;
friend class CUDTUnited;

public:
   CRcvQueue();
   ~CRcvQueue();

public:

      // Functionality:
      //    Initialize the receiving queue.
      // Parameters:
      //    1) [in] size: queue size
      //    2) [in] mss: maximum packet size
      //    3) [in] hsize: hash table size
      //    4) [in] c: UDP channel to be associated to the queue
      //    5) [in] t: timer
      // Returned value:
      //    None.

   void init(const int& size, const int& payload, const int& hsize, const CChannel* c, const CTimer* t);

      // Functionality:
      //    Read a packet for a specific UDT socket id.
      // Parameters:
      //    1) [out] addr: source address of the packet
      //    2) [out] packet: received packet
      //    3) [in] id: Socket ID
      // Returned value:
      //    Data size of the packet

   int recvfrom(sockaddr* addr, CPacket& packet, const int32_t& id);

private:
#ifndef WIN32
   static void* enQueue(void* param);
   static void* deQueue(void* param);
#else
   static DWORD WINAPI enQueue(LPVOID param);
   static DWORD WINAPI deQueue(LPVOID param);
#endif

   pthread_t m_enQThread;
   pthread_t m_deQThread;

private:
   CUnit* m_pUnitQueue;		// The received packet queue
   int m_iQueueLen;		// Size of the queue
   int m_iUnitSize;		// Size of the storage per queue entry
   int m_iPtr;			// Next available writing entry

   CUnit** m_pActiveQueue;	// Queue for data packets
   int m_iAQHeadPtr;		// Header pointer for AQ, first available packet
   int m_iAQTailPtr;		// Tail pointer for AQ, next avilable slot

   CUnit** m_pPassiveQueue;	// Queue for control packets
   int m_iPQHeadPtr;		// Header pointer for PQ, first available packet
   int m_iPQTailPtr;		// Tail pointer for PQ, next avilable slot

private:
   CRcvUList* m_pRcvUList;	// List of UDT instances that will read packets from the queue
   CHash* m_pHash;		// Hash table for UDT socket looking up
   CChannel* m_pChannel;	// UDP channel for receving packets
   CTimer* m_pTimer;		// shared timer with the snd queue

private:
   pthread_mutex_t m_PassLock;
   pthread_cond_t m_PassCond;

   pthread_cond_t m_QueueCond;
   pthread_mutex_t m_QueueLock;

private:
   volatile UDTSOCKET m_ListenerID;	// The only listening socket that is associated to the queue, if there is one

   int m_iPayloadSize;			// packet payload size
};


class CMultiplexer
{
public:
   CSndQueue* m_pSndQueue;	// The sending queue
   CRcvQueue* m_pRcvQueue;	// The receiving queue
   CChannel* m_pChannel;	// The UDP channel for sending and receiving
   CTimer* m_pTimer;		// The timer

   int m_iPort;			// The UDP port number of this multiplexer
   int m_iIPversion;		// IP version
   int m_iMTU;			// MTU
   int m_iSockType;		// Socket Type
   int m_iRefCount;		// number of UDT instances that are associated with this multiplexer
};

#endif

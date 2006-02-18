/*****************************************************************************
Copyright © 2001 - 2006, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) version 3

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
This header file contains the definitions of common types and utility classes.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 02/14/2006
*****************************************************************************/

#ifndef __UDT_COMMON_H__
#define __UDT_COMMON_H__


#ifndef WIN32
   #include <sys/time.h>
   #include <sys/uio.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
#else
   #include <windows.h>
#endif
#include "udt.h"


////////////////////////////////////////////////////////////////////////////////

#ifdef WIN32
   // Windows compability
   typedef HANDLE pthread_t;
   typedef HANDLE pthread_mutex_t;
   typedef HANDLE pthread_cond_t;
   typedef DWORD pthread_key_t;

   struct iovec
   {
      __int32 iov_len;
      char* iov_base;
   };

   int gettimeofday(timeval *tv, void*);
   int readv(SOCKET s, const iovec* vector, int count);
   int writev(SOCKET s, const iovec* vector, int count);
#endif

////////////////////////////////////////////////////////////////////////////////

class CTimer
{
public:

      // Functionality:
      //    Sleep for "interval" CCs.
      // Parameters:
      //    0) [in] interval: CCs to sleep.
      // Returned value:
      //    None.

   void sleep(const unsigned __int64& interval);

      // Functionality:
      //    Seelp until CC "nexttime".
      // Parameters:
      //    0) [in] nexttime: next time the caller is waken up.
      // Returned value:
      //    None.

   void sleepto(const unsigned __int64& nexttime);

      // Functionality:
      //    Stop the sleep() or sleepto() methods.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void interrupt();

public:

      // Functionality:
      //    Read the CPU clock cycle into x.
      // Parameters:
      //    0) [out] x: to record cpu clock cycles.
      // Returned value:
      //    None.

   static void rdtsc(unsigned __int64 &x);

      // Functionality:
      //    return the CPU frequency.
      // Parameters:
      //    None.
      // Returned value:
      //    CPU frequency.

   static unsigned __int64 getCPUFrequency();

private:
   unsigned __int64 m_ullSchedTime;             // next schedulled time

private:
   static unsigned __int64 s_ullCPUFrequency;   // CPU frequency : clock cycles per microsecond
   static unsigned __int64 readCPUFrequency();
};

////////////////////////////////////////////////////////////////////////////////

class CGuard
{
public:
   CGuard(pthread_mutex_t& lock);
   ~CGuard();

private:
   pthread_mutex_t& m_Mutex;    // Alias name of the mutex to be protected
   __int32 m_iLocked;           // Locking status

   void operator = (const CGuard&) {}
};

////////////////////////////////////////////////////////////////////////////////

class CSeqNo
{
public:
   inline static const __int32 seqcmp(const __int32& seq1, const __int32& seq2)
   {return (abs(seq1 - seq2) < m_iSeqNoTH) ? (seq1 - seq2) : (seq2 - seq1);}

   inline static const __int32 seqlen(const __int32& seq1, const __int32& seq2)
   {return (seq1 <= seq2) ? (seq2 - seq1 + 1) : (seq2 - seq1 + 1 + m_iMaxSeqNo);}

   inline static const __int32 incseq(const __int32& seq)
   {return (seq == m_iMaxSeqNo) ? 0 : seq + 1;}

   inline static const __int32 decseq(const __int32& seq)
   {return (seq == 0) ? m_iMaxSeqNo : seq - 1;}

private:
   static const __int32 m_iSeqNoTH = 0x3FFFFFFF;        // threshold for comparing seq. no.
   static const __int32 m_iMaxSeqNo = 0x7FFFFFFF;       // maximum sequence number used in UDT
};


#endif

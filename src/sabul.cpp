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
sabul.cpp
The implementation of the protocol common operations

Author: Yunhong Gu [gu@lac.uic.edu]
Last Update: Jan. 07, 2003
********************************************************************/



#include "sabul.h"
#include <unistd.h>

//
// Some of the most important paramters are initialized here.
// They are NOT changable.
//
// They will be changable in future version of SABUL 3.0.
//


CSabul::CSabul():
m_iSYNInterval(10000),
m_iDefaultPktSize(1472),
m_iDefaultPort(7000),
m_lMaxSeqNo(100000000),
m_lSeqNoTH(50000000),
m_lSendBufSize(40960000),
m_lRecvBufSize(40960000)
{
   m_bBlockable = false;
   m_iDataPort = m_iCtrlPort = m_iDefaultPort;
   m_bConnection = false;

   m_ullCPUFrequency = getCPUFrequency();

   m_iPktSize = m_iDefaultPktSize;
   m_iPayloadSize = m_iPktSize - sizeof(long);
   m_iMaxLossLength = m_iPktSize / sizeof(long) - 2;

   m_pDCThread = new pthread_t;
}

CSabul::~CSabul()
{
   delete m_pDCThread;
}

void CSabul::setOpt(const SabulOption& opt)
{
   m_iDataPort = opt.m_iDataPort;
   m_iCtrlPort = opt.m_iCtrlPort;
   m_iPktSize = opt.m_iPktSize;
   m_iPayloadSize = opt.m_iPayloadSize;
   m_iMaxLossLength = opt.m_iMaxLossLength;
   m_bBlockable = bool(opt.m_iBlockable);
}

void CSabul::getOpt(SabulOption& opt) const
{
   opt.m_iDataPort = m_iDataPort;
   opt.m_iCtrlPort = m_iCtrlPort;
   opt.m_iPktSize = m_iPktSize;
   opt.m_iPayloadSize = m_iPayloadSize;
   opt.m_iMaxLossLength = m_iMaxLossLength;
   opt.m_iBlockable = int(m_bBlockable);
}

//
//currently, only SBL_BLOCK and SBL_VERBOSE are meaningful.
//see comments in sabul.h
//

int CSabul::setOpt(SblOpt optName, const void* optval, const int& optlen)
{
   if (m_bConnection)
      throw int(-1);
 
   switch (optName)
   {
   case SBL_BLOCK:
      m_bBlockable = *((bool *)optval);
      break;

   case SBL_DPORT:
      m_iDataPort = *((int *)optval);
      break;
   
   case SBL_CPORT:
      m_iCtrlPort = *((int *)optval);
      break;
   
   case SBL_SNDBUF:
      m_lSendBufSize = *((long *)optval);
      break;
   
   case SBL_RCVBUF:
      m_lRecvBufSize = *((long *)optval);
      break;

   case SBL_PKTSIZE:
      m_iPktSize = *((int *)optval);
      break;

   case SBL_RCVFLAGSIZE:
      break;

   case SBL_VERBOSE:
      m_iVerbose = *((int *)optval);
      break;

   default:
      break;
   }

   return 0;
}
   
int CSabul::getOpt(SblOpt optName, void* optval, int& optlen)
{
   switch (optName)
   {
   case SBL_BLOCK:
      *((bool *)optval) = m_bBlockable;
      optlen = sizeof(bool);
      break;

   case SBL_DPORT:
      *((int *)optval) = m_iDataPort;
      optlen = sizeof(int);
      break;

   case SBL_CPORT:
      *((int *)optval) = m_iCtrlPort;
      optlen = sizeof(int);
      break;

   case SBL_SNDBUF:
      *((long *)optval) = m_lSendBufSize;
      optlen = sizeof(long);
      break;

   case SBL_RCVBUF:
      *((long *)optval) = m_lRecvBufSize;
      optlen = sizeof(long);
      break;

   case SBL_PKTSIZE:
      *((int *)optval) = m_iPktSize;
      optlen = sizeof(int);
      break;

   case SBL_RCVFLAGSIZE:
      optlen = sizeof(int);
      break;

   case SBL_VERBOSE:
      optlen = sizeof(int);
      break;

   default:
      break;
   }

   return 0;
}

//
// This function (and the three below) uses Intel rdtsc instruction
// to get high precision timer. 
//
// Use similar instruction on other architecture.
//
// Use "gettimeofday" system call is also ok. CSabul::getCPUFrequency()
// should return 1 if "gettimeofday" is used in CSabul::rdtsc().
//

void CSabul::rdtsc(unsigned long long int &x)
{
   __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
}

//
// get the number of the clock cycles in 1 microsecond.
// 
//

unsigned long long int CSabul::getCPUFrequency()
{
   unsigned long long int t1, t2;

   rdtsc(t1);
   usleep(100000);
   rdtsc(t2);

   return (t2 - t1) / 100000;
}

void CSabul::sleep(unsigned long long int interval)
{
   if (interval <= 0)
      return;

   unsigned long long int t1, t2;
   rdtsc(t1);
   rdtsc(t2);
   while (t2 - t1 < interval)
      rdtsc(t2);
}

void CSabul::sleepto(unsigned long long int nexttime)
{
   unsigned long long int t;
   rdtsc(t);
   while (t < nexttime)
   {
      __asm__ volatile ("nop; nop; nop; nop; nop;");
      rdtsc(t);
   }
}

///////////////////////////////////////////////////////////////////

//
// The CGuard class automatically lock and unlock a MUTEX variable in 
// its construture and destructure.
//

CGuard::CGuard(pthread_mutex_t& lock):
m_Mutex(lock)
{
   m_iLocked = pthread_mutex_lock(&m_Mutex);
}

CGuard::~CGuard()
{
   if (0 == m_iLocked)
      pthread_mutex_unlock(&m_Mutex);
}

//

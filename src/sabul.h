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
sabul.h
The protocol common data structure and operation definitions

Author: Yunhong Gu [gu@lac.uic.edu]
Last Update: Jan. 07, 2003
********************************************************************/



#ifndef _SABUL_H_
#define _SABUL_H_

#include <pthread.h>
#include <sys/time.h>
#include <fstream>
#include "channel.h"
#include "packet.h"


// 
// Currently the only modifiable options are m_iBlockable (SBL_BLOCK) and m_iVerbose;
// The others are to be completed in future SABUL 3.0. 
//

struct SabulOption
{
   int m_iDataPort;
   int m_iCtrlPort;

   int m_iPktSize;
   int m_iPayloadSize;
   int m_iMaxLossLength;

   //
   // If this value is 1, SABUL enables blocking sending and the sent data buffer  
   // is NOT auto-released. If it is 0, the sending is non-blocking and data will
   // will be released after it is sent.
   //
   int m_iBlockable;

   long m_lSendBufSize;

   long m_lRecvBufSize;
   int m_iRecvFlagSize;

   //
   // Output the debug information to screen (1) or a log file (0). 
   //
   int m_iVerbose;
};

enum SblOpt {SBL_BLOCK = 1, SBL_DPORT, SBL_CPORT, SBL_SNDBUF, SBL_RCVBUF, SBL_PKTSIZE, SBL_RCVFLAGSIZE, SBL_VERBOSE};

class CSabul
{
public:
   CSabul();
   virtual ~CSabul();
   
   virtual int open(const int& port = -1) {return m_iDefaultPort;}
   virtual int open(const char* ip, const int& port = -1) {return m_iDefaultPort;}
   virtual void close() = 0;

   virtual bool send(char* data, const long& len) const {return false;}
   virtual long recv(char* data, const long& len) const {return 0;}

   virtual bool sendfile(const int& fd, const int& offset, const int& size) const {return false;}
   virtual long recvfile(const int& fd, const int& offset, const int& size) const {return 0;}

   virtual void setOpt(const SabulOption& opt);
   virtual void getOpt(SabulOption& opt) const;

   virtual int setOpt(SblOpt optName, const void* optval, const int& optlen);
   virtual int getOpt(SblOpt optName, void* optval, int& optlen);

protected:
   void rdtsc(unsigned long long int &x);
   unsigned long long int getCPUFrequency();
   void sleep(unsigned long long int interval);
   void sleepto(unsigned long long int nexttime);

protected:
   int m_iSYNInterval;

   CChannel* m_pDataChannel;
   CChannel* m_pCtrlChannel;

   pthread_t* m_pDCThread;

   volatile bool m_bClosing;

   unsigned long long int m_ullCPUFrequency;
 
   int m_iRTT;

   int m_iPktSize;
   int m_iPayloadSize;
   int m_iMaxLossLength;

   int m_iDataPort;
   int m_iCtrlPort;

   bool m_bBlockable;

   SabulOption m_RemoteOpt;

   enum PktType {SC_ACK = 1, SC_ERR, SC_EXP, SC_SYN};

   const int m_iDefaultPktSize;
   const int m_iDefaultPort; 

   const long m_lMaxSeqNo;
   const long m_lSeqNoTH;

   bool m_bConnection;

   long m_lSendBufSize;
   long m_lRecvBufSize;

   int m_iVerbose;
   ofstream m_Log;
};

//
// This is a facility class to provide MUTEX lock and unlock
// to a whole class method/function.
//

class CGuard
{
public:
   CGuard(pthread_mutex_t& lock);
   ~CGuard();

private:
   pthread_mutex_t& m_Mutex;
   int m_iLocked;
};

#endif

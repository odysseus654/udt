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
sender.h
The sender side class definitions

Author: Yunhong Gu [gu@lac.uic.edu]
Last Update: Jan. 08, 2003
********************************************************************/



#ifndef __SABUL_SENDER_H_
#define __SABUL_SENDER_H_

#include "sabul.h"

class CSabulSender: public CSabul
{
public:
   CSabulSender();
   virtual ~CSabulSender(); 

   int open(const int& port = -1);
   void listen();
   void close();

   bool send(const char* data, const long& len);
   bool sendfile(const int& fd, const int& offset, const int& size);

   long getCurrBufSize() const;

   //
   // Set the initial, minimum, and maximum sending rate
   //
   void setRate(const double& initrate, const double& l_limit = 0, const double& u_limit = 1000); //in Mbps

private:
   static void* dcHandler(void* sender);

   void processFeedback(const PktType type, const long attr = 0, const long* = NULL);
   void rateControl(const double& currlossrate);

private:
   pthread_cond_t m_SendDataCond;
   pthread_mutex_t m_SendDataLock;

   pthread_cond_t m_SendBlockCond;
   pthread_mutex_t m_SendBlockLock;

   double m_dInterval;
   unsigned long long int m_ullInterval;

   // The lower and upper limit of the inter-packet interval, in microseconds.
   double m_dLowerLimit;
   double m_dUpperLimit;

   // The m_iSendFlagSize is like the TCP window and it limits the maximum packets on flight, but it is not changing
   int m_iSendFlagSize;
   // Same as m_iSendFlagSize, will be obsolete in next version of SABUL.
   int m_iWarningLine;

   int m_iERRCount;
   int m_iDecCount;

   long m_lLastAck;
   long m_lLocalSend;
   long m_lLocalERR;
   long m_lLastSYNSeqNo;
   long m_lCurrSeqNo;

   int m_iEXPInterval;

   // For rate control.
   double m_dLossRateLimit;
   const double m_dWeight;
   double m_dHistoryRate;

   timeval m_LastSYNTime;

   long m_lLastDecSeq;

   bool m_bFreeze;

//
// The sender side buffer management module.
// All the buffers to be sent are linked together.
// m_pCurrAckBlk and m_pBlock are the first block of the list.
// m_pCurrAckPnt is the pointer pointing to the position prior to which 
// all the data has been acknowledged. Once the m_pCurrAckPnt moves to
// the next block, the current m_pCurrAckBlk is removed from the list
// and points to the next block.
// m_pCurSendBlk is the block where the sender looks for next first-time data
// packet to send. m_lCurrSendPnt is the pointer to the position that
// that the last sent out data.
//

   class CBuffer
   {
   public:
      CBuffer();
      CBuffer(const long& bufsize);
      ~CBuffer();

      //
      // Add the application buffer of "data" with length of "len" to the linked list
      // If "map" is false, the buffer will be released by SABUL after it is sent out,
      // Otherwise it is "munmap"ed.
      // 
      // The method return true if the total size of the data waiting to be sent is
      // not excedding a limit (m_lSizeLimit), otherwise it returns false.
      //
      bool addBuffer(const char* data, const long& len, const bool map = false);

      //
      // Read "len" size of data to the buffer of "*data" from the m_pCurSendBlk
      // starting from m_lCurrSendpnt.
      //
      // Return the actual size of data read.
      //
      long readData(char** data, const long& len);
   
      //
      // Read "len" size of data to the buffer of "*data" from the m_pCurrAckPnt + offset.
      //
      long readData(char** data, const long offset, const long& len);

      //
      // The "len" size of data has been acknowledged, the m_pCurrAckPnt and m_pCurrAckBlock may
      // need to be updated.
      //
      // SABUL always try to pack fixed size ("payloadsize") of packet. If "payloadsize" cannot
      // devide the length of the block, some modifications need to be done.
      //
      void ackData(const long& len, const int& payloadsize);

      long getCurrBufSize() const;
      long getSizeLimit() const;

   private:
      pthread_mutex_t m_BufLock;

      struct Block
      {
         char* m_pcData;
         long m_lLength;
         bool m_bMap;

         Block* m_next;
      } *m_pBlock, *m_pLastBlock, *m_pCurrSendBlk, *m_pCurrAckBlk;

      long m_lCurrBufSize;
      long m_lCurrSendPnt;
      long m_lCurrAckPnt;

      long m_lSizeLimit;
   } *m_pBuffer; 


//
// the CLossList class is to store the sequence numbers of the lost packets.
// The seq. no. are stored in ascending order in the list.
// 

   class CLossList
   {
   public:
      CLossList();
      CLossList(const long& seqno);

      int insert(const long& seqno);
      //
      // remove all the seq. no. prior to the "seqno" specified in the parameter
      // when an acknowledgement arrives with the "seqno" in its ATTR filed.
      //
      void remove(const long& seqno);

      int getLossLength() const;
      //
      // Read the seq. no. of the first node and remove it from the list.
      //
      long getLostSeq();

   private:
      //
      // the m_lAttr in the first node in the total number of the lost packets in the list.
      //
      long m_lAttr;
      CLossList *m_next;
   } *m_pLossList;
};


#endif

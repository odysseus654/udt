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
recver.h
The SABUL receiver class definitions

Author: Yunhong Gu [gu@lac.uic.edu]
Last Update: Jan. 07, 2003
********************************************************************/


#ifndef __SABUL_RECVER_H_
#define __SABUL_RECVER_H_

#include <bitset>
#include "sabul.h"

class CSabulRecver: public CSabul
{
public:
   CSabulRecver();
   virtual ~CSabulRecver();

   int open(const char* ip, const int& port);
   void close();

   long recv(char* data, const long& len);
   long recvfile(const int& fd, const int& offset, const int& size);

private:
   static void* dcHandler(void* recver);
   void feedback(PktType type, const int& len = 0, const long* data = NULL);
   void getNextExpect(const long& offset);
   
private:
   // The bitset must use be fixed size at initial time, so we use a fixed enum value
   // It will not be used in future version of SABUL
   // m_RecvFlag is a reorder window
   enum SizeType {FlagSize = 25600};
   bitset<FlagSize> m_RecvFlag;

   long m_lLastAck;
   long m_lCurrSeqNo;
   long m_lNextExpect;

   int m_iACKInterval;
   int m_iERRInterval;

   long m_lLocalRecv;

   pthread_cond_t m_RecvDataCond;
   pthread_mutex_t m_RecvDataLock;

   //These variables are for temporally store the parameters from "recv" method 
   volatile bool m_bReadBuf;
   volatile char* m_pcTempData;
   volatile long m_lTempLen;

   //
   // The receiver side buffer management module
   // 
   // The buffer is a logically circular memory block, with three pointers of m_lStartPos, m_LastAckPos, and m_lMaxOffset.
   //
   // If a "recv" method is called and there is no enough data to meet the requested size, the user buffer is "registered" to
   // the protocol buffer. This equivelantly increase the protocol buffer size logically. New received data will be put into the
   // user buffer directly.
   class CBuffer
   {
   public:
      CBuffer();
      CBuffer(const long& bufsize);
      ~CBuffer();

      // The receiver speculates the next data position
      bool nextDataPos(char** data, long offset, const long& len);

      // If the speculation fails, copy the received data explicitly into the buffer
      bool addData(char* data, long offset, long len);

      // Again, SABUL uses fixed packet size, if an unusual packets comes, the data in the buffer may need to be moved.
      void moveData(const long& offset, const long& len);

      // Read data into user buffer in a "recv" call
      bool readBuffer(char* data, const long& len);

      // Update the m_lLastAckPos
      int ackData(const long& len);

      // Check if the user buffer has been fullfiled.
      bool reachUserBufBoundary();

      void registerUserBuf(char* buf, const long& len);

      long getSizeLimit() const;

   private:
      char* m_pcData;
      long m_lSize;

      long m_lStartPos;
      long m_lLastAckPos;
      long m_lMaxOffset;

      char* m_pcUserBuf;
      long m_lUserBufSize;
      long m_lUserBufAck;
      bool m_bAfterUserBufBoundary;
   } *m_pBuffer;

   //
   // The list used to record the irregular packet - packet is not in the fixed SABUL packet size
   // Seq. no. is stored in the list ascendingly.
   //
   class CIrregularPktList
   {
   public:
      CIrregularPktList();
      ~CIrregularPktList();
 
      long currErrorSize() const;
      long currErrorSize(const long& seqno) const;

      void addIrregularPkt(const long& seqno, const int& errsize);
      void deleteIrregularPkt(const long& seqno);      

   private:
      long m_lSeqNo;
      int m_iErrorSize;
 
      CIrregularPktList* m_next;
   } *m_pIrregularPktList;

   //
   // The loss list used to record the seq. no. of the lost packets
   // Seq. no. is stored in the list ascendingly.
   //
   class CLossList
   {
   public:
      CLossList();
      CLossList(const long& seqno);

      void insert(const long& seqno);
      void remove(const long& seqno);
      int getLossLength() const;
      long getFirstLostSeq() const;
      //
      // Find those seq. no. that have not been reported in the last "interval" timer and copy them
      // into the buffer "array", which will be sent back to the sender. The number of the found packets
      // is recorded in "len". The maximum of seq. no. in the result is "limit".
      //
      void getLossArray(long* array, int& len, const int& limit, const long& interval) const;
   private:
      //
      // Each node of the loss list records the seq. no, the last feedback time, the pointers of the next and last nodes.
      //
      long m_lAttr;
      timeval m_LastFeedbackTime;
      CLossList *m_next;
      CLossList *m_tail;
   } *m_pLossList;
};


#endif


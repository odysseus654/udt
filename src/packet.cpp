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
packet.cpp
Data and control packet operations

Author: Yunhong Gu [gu@lac.uic.edu]
Last Update: Oct 08, 2002
********************************************************************/



#include "packet.h"

template <class T> CPacket<T>::CPacket(const int& len):
m_iLength(len)
{
   m_pData = new T [m_iLength / sizeof(T)];
}

template <class T> CPacket<T>::~CPacket()
{
   delete [] m_pData;
}

template <class T> inline int CPacket<T>::getLength() const
{
   return m_iLength * sizeof(T);
}

template <class T> inline void CPacket<T>::setLength(const int& len)
{
   m_iLength = len / sizeof(T);
}

template <class T> inline T* CPacket<T>::getData() const
{
   return m_pData;
}

/////////////////////////////////////////////////////////////////////
//

CPktSblData::CPktSblData(const int& len): CPacket<char>(len),
m_lSeqNo(*(long *)m_pData),
m_pcData(m_pData + sizeof(long))
{
}

/////////////////////////////////////////////////////////////////////
//

CPktSblCtrl::CPktSblCtrl(const int& len): CPacket<long>(len),
m_lPktType(*m_pData),
m_lAttr(*(m_pData + 1)),
m_plData(m_pData + 2)
{
   m_lAttr = 0;
}

/////////////////////////////////////////////////////////////////////
//

CPacketVector::CPacketVector(const int& len):
m_lSeqNo(m_lSeqNoVec),
m_pcData((char *)(m_PacketVector[1].iov_base))
{
   m_PacketVector[0].iov_base = &m_lSeqNoVec;
   m_PacketVector[0].iov_len = sizeof(long);
   m_PacketVector[1].iov_len = len;
}

CPacketVector::~CPacketVector()
{
}

int CPacketVector::getLength() const
{
   return m_PacketVector[1].iov_len;
}

void CPacketVector::setLength(const int& len)
{
   m_PacketVector[1].iov_len = len;
}

inline void CPacketVector::pack(const long& seqno, const char* pcdata, const int& size)
{
   m_PacketVector[0].iov_base = (void *)&seqno;
   m_PacketVector[1].iov_base = (void *)pcdata;
   m_PacketVector[1].iov_len = size;
}

iovec* CPacketVector::getPacketVector() const
{
   return m_PacketVector;
}

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
channel.cpp
Methods for data and control connections

Author: Yunhong Gu [gu@lac.uic.edu]
Last Update: Mar. 16, 2003
********************************************************************/



#include <netdb.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include "channel.h"

//
// The buffer size is fixed in the current version.
// They will be modifiable in the next version.
//

CChannel::CChannel():
m_iBacklog(10),
m_uiSendBufSize(64 * 1024),
m_uiRecvBufSize(4 * 1024 * 1024)
{
   m_SockAddr.sin_family = AF_INET;
   m_SockAddr.sin_port = 0;
   m_SockAddr.sin_addr.s_addr = INADDR_ANY;
   memset(&(m_SockAddr.sin_zero), '\0', 8);
}

CChannel::~CChannel()
{
}

void CChannel::disconnect() const
{
   close(m_iSocket);
}

////////////////////////////////////////////////////////////////////////
//

CUdpChannel::CUdpChannel(): CChannel()
{
   m_iSocket = socket(AF_INET, SOCK_DGRAM, 0);
   if (-1 == bind(m_iSocket, (sockaddr *)&m_SockAddr, sizeof(sockaddr)))
      throw int(errno);
}

CUdpChannel::CUdpChannel(int& port): CChannel()
{
   m_SockAddr.sin_port = htons(port);
   m_iSocket = socket(AF_INET, SOCK_DGRAM, 0);
   if (0 == bind(m_iSocket, (sockaddr *)&m_SockAddr, sizeof(sockaddr)))
      return;
 
   //
   // If the assigned port is not available, find any free port
   //
   m_SockAddr.sin_port = 0;
   if (-1 == bind(m_iSocket, (sockaddr *)&m_SockAddr, sizeof(sockaddr)))
      throw int(errno);

   sockaddr_in *name = new sockaddr_in;
   unsigned int namelen = sizeof(sockaddr);
   getsockname(m_iSocket, (sockaddr *)name, &namelen);
   port = ntohs(name->sin_port);
   delete name;
}

CUdpChannel::CUdpChannel(const char* ip, int& port): CChannel()
{
   m_SockAddr.sin_port = htons(port);
   m_SockAddr.sin_addr.s_addr = inet_addr(ip);
   m_iSocket = socket(AF_INET, SOCK_DGRAM, 0);
   if (0 == bind(m_iSocket, (sockaddr *)&m_SockAddr, sizeof(sockaddr)))
      return;

   //
   // If the assigned port is not available, find any free port
   //
   m_SockAddr.sin_port = 0;
   if (-1 == bind(m_iSocket, (sockaddr *)&m_SockAddr, sizeof(sockaddr)))
      throw int(errno);

   sockaddr_in *name = new sockaddr_in;
   unsigned int namelen = sizeof(sockaddr);
   getsockname(m_iSocket, (sockaddr *)name, &namelen);
   port = ntohs(name->sin_port);
   delete name;
}

CUdpChannel::~CUdpChannel()
{
}

CUdpChannel& CUdpChannel::operator<<(CPktSblData& sdpkt) const
{
   write(m_iSocket, sdpkt.getData(), sdpkt.getLength());
   //::send(m_iSocket, sdpkt.getData(), sdpkt.getLength(), 0);
   return (CUdpChannel &)(*this);
}

CUdpChannel& CUdpChannel::operator>>(CPktSblData& sdpkt) const
{
   int len = read(m_iSocket, sdpkt.getData(), sdpkt.getLength());
   //int len = ::recv(m_iSocket, sdpkt.getData(), sdpkt.getLength(), 0);
   sdpkt.setLength(len);
   return (CUdpChannel &)(*this);
}

CUdpChannel& CUdpChannel::operator<<(CPacketVector& sdpkt) const
{   
   // send iovec structure
   writev(m_iSocket, sdpkt.getPacketVector(), 2);
   return const_cast<CUdpChannel &>(*this);
}

CUdpChannel& CUdpChannel::operator>>(CPacketVector& sdpkt) const
{
   // receive iovec structure
   sdpkt.setLength(readv(m_iSocket, sdpkt.getPacketVector(), 2));
   return const_cast<CUdpChannel &>(*this);
}

int CUdpChannel::send(char* buffer, const int& size) const
{ 
   return ::send(m_iSocket, buffer, size, 0);
}

int CUdpChannel::recv(char* buffer, const int& size) const
{
   return ::recv(m_iSocket, buffer, size, 0);
}

int CUdpChannel::peek(char* buffer, const int& size) const
{
   return ::recv(m_iSocket, buffer, size, MSG_PEEK);
}

void CUdpChannel::connect(const char* ip, const int& port)
{
   m_RemoteSockAddr.sin_family = AF_INET;
   m_RemoteSockAddr.sin_port = htons(port);
   hostent *remoteaddr = gethostbyname(ip);
   if (NULL == remoteaddr)
      throw int(errno);
   m_RemoteSockAddr.sin_addr = *((in_addr *)remoteaddr->h_addr);
   memset(&(m_RemoteSockAddr.sin_zero), '\0', 8);
   if (-1 == ::connect(m_iSocket, (sockaddr *)(&m_RemoteSockAddr), sizeof(sockaddr)))
      throw int(errno);
   setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, &m_uiSendBufSize, sizeof(unsigned int));
}

const sockaddr_in& CUdpChannel::connect()
{
   setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, &m_uiRecvBufSize, sizeof(unsigned int));


   //Set receiving waiting time 100us
   timeval tv;
   tv.tv_sec = 0;
   tv.tv_usec = 100;
   setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(timeval));

   return m_RemoteSockAddr;
}


/////////////////////////////////////////////////////////////////////////
//

CTcpChannel::CTcpChannel():
m_lTimerOut(1000000)
{
   m_iSocket = socket(AF_INET, SOCK_STREAM, 0);
   if (-1 == bind(m_iSocket, (sockaddr *)(&m_SockAddr), sizeof(sockaddr)))
      throw int(errno);

   m_pPollFD = new pollfd;
}

CTcpChannel::CTcpChannel(int& port):
m_lTimerOut(1000000)
{
   m_SockAddr.sin_port = htons(port);
   m_iSocket = socket(AF_INET, SOCK_STREAM, 0);

   //using for poll().
   m_pPollFD = new pollfd;

   if (0 == bind(m_iSocket, (sockaddr *)(&m_SockAddr), sizeof(sockaddr)))
      return;

   //
   // If the assigned port is not available, find any free port
   //
   m_SockAddr.sin_port = 0;
   if (-1 == bind(m_iSocket, (sockaddr *)(&m_SockAddr), sizeof(sockaddr)))
      throw int(errno);

   sockaddr_in *name = new sockaddr_in;
   unsigned int namelen = sizeof(sockaddr);
   getsockname(m_iSocket, (sockaddr *)name, &namelen);
   port = ntohs(name->sin_port);
   delete name;
}

CTcpChannel::CTcpChannel(const char* ip, int& port):
m_lTimerOut(1000000)
{
   m_SockAddr.sin_addr.s_addr = inet_addr(ip);
   m_SockAddr.sin_port = htons(port);
   m_iSocket = socket(AF_INET, SOCK_STREAM, 0);

   //using for poll().
   m_pPollFD = new pollfd;

   if (0 == bind(m_iSocket, (sockaddr *)(&m_SockAddr), sizeof(sockaddr)))
      return;

   //
   // If the assigned port is not available, find any free port
   //
   m_SockAddr.sin_port = 0;
   if (-1 == bind(m_iSocket, (sockaddr *)(&m_SockAddr), sizeof(sockaddr)))
      throw int(errno);

   sockaddr_in *name = new sockaddr_in;
   unsigned int namelen = sizeof(sockaddr);
   getsockname(m_iSocket, (sockaddr *)name, &namelen);
   port = ntohs(name->sin_port);
   delete name;
}

CTcpChannel::~CTcpChannel()
{
   delete m_pPollFD;
}

CTcpChannel& CTcpChannel::operator<<(CPktSblCtrl& scpkt) const
{
   send((char *)(scpkt.getData()), scpkt.getLength());
   return (CTcpChannel &)(*this);
}

CTcpChannel& CTcpChannel::operator>>(CPktSblCtrl& scpkt) const
{
   int recvlen = poll(m_pPollFD, 1, 0);

   if (0 < recvlen)
   {
      recvlen = recv((char *)(scpkt.getData()), 2 * sizeof(long));
      if (2 == scpkt.m_lPktType)
         recvlen = recv((char *)(scpkt.getData() + 2), scpkt.m_lAttr * sizeof(long));
   }

   scpkt.setLength(recvlen);

   return (CTcpChannel &)(*this);
}

int CTcpChannel::send(char* buffer, const int& size) const
{
   for (int i = 0; i < size / 4; i ++)
      *((long *)buffer + i) = htonl(*((long *)buffer + i));

   int sentsize = 0;
   int tosend = size;
   do
   {
      sentsize += ::send(m_iSocket, buffer + sentsize, tosend, 0);
      tosend = size - sentsize;
   } while (sentsize != size);

   for (int i = 0; i < size / 4; i ++)
      *((long *)buffer + i) = ntohl(*((long *)buffer + i));

   return size;
}

int CTcpChannel::recv(char* buffer, const int& size) const
{
   int len;
   int recvsize = 0;
   int torecv = size;

   timeval s, t;

   gettimeofday(&s, 0);

   do
   {
      len = ::recv(m_iSocket, buffer + recvsize, torecv, 0);

      if (len > 0)
      {
         recvsize += len;
         gettimeofday(&s, 0);
      }
      else 
      {
         gettimeofday(&t, 0);
         if (((t.tv_sec - s.tv_sec) * 1000000 + (t.tv_usec - s.tv_usec)) > m_lTimerOut)
            return -1;
      }

      torecv = size - recvsize;
   } while (recvsize != size);

   for (int i = 0; i < size / 4; i ++)
      *((long *)buffer + i) = ntohl(*((long *)buffer + i));

   return size;
}

void CTcpChannel::connect(const char* ip, const int& port)
{
   m_RemoteSockAddr.sin_family = AF_INET;
   m_RemoteSockAddr.sin_port = htons(port);
   hostent *remoteaddr = gethostbyname(ip);
   if (NULL == remoteaddr)
      throw int(errno);
   m_RemoteSockAddr.sin_addr = *((in_addr *) remoteaddr->h_addr);
   memset(&(m_RemoteSockAddr.sin_zero), '\0', 8);

   if (-1 == ::connect(m_iSocket, (sockaddr *)(&m_RemoteSockAddr), sizeof(sockaddr)))
      throw int(errno);

   // set TCP no_delay
   unsigned int tcpnodelay = 1;
   setsockopt(m_iSocket, SOL_TCP, TCP_NODELAY, &tcpnodelay, sizeof(unsigned int));
}

const sockaddr_in& CTcpChannel::connect()
{
   listen(m_iSocket, m_iBacklog);
   socklen_t addrsize = sizeof(sockaddr);
   int origsock = m_iSocket;
   m_iSocket = accept(m_iSocket, (sockaddr *)&m_RemoteSockAddr, &addrsize);
   close(origsock);

   m_pPollFD->fd = m_iSocket;
   m_pPollFD->events = POLLIN;

   // set TCP non-blocking
   int opts = fcntl(m_iSocket, F_GETFL);
   fcntl(m_iSocket, F_SETFL, opts | O_NONBLOCK);

   return m_RemoteSockAddr;
}

//

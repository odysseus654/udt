/*****************************************************************************
Copyright � 2001 - 2004, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) version 2

Laboratory for Advanced Computing (LAC)
National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.lac.uic.edu/

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software (UDT) and associated documentation files (the
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
THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*****************************************************************************/

/*****************************************************************************
This file contains the implementation of UDT packet sending and receiving
routines.

UDT uses UDP for packet transfer. Data gathering/scattering is used in
both sending and receiving.

reference:
socket programming reference, writev/readv
UDT packet definition: packet.h
*****************************************************************************/

/*****************************************************************************
written by 
   Yunhong Gu [ygu@cs.uic.edu], last updated 09/29/2004
*****************************************************************************/

#ifndef WIN32
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <unistd.h>
   #include <fcntl.h>
   #include <cstring>
   #include <cstdio>
   #include <cerrno>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
#endif

#include "udt.h"

using namespace std;


// For BSD/WIN32 compatability
#ifdef BSD
   #define socklen_t int
#elif WIN32
   #define socklen_t int
#endif

#ifndef WIN32
   #define NET_ERROR errno
#else
   #define NET_ERROR WSAGetLastError()
#endif


CChannel::CChannel():
m_iIPversion(4),
m_iSndBufSize(102400),
m_iRcvBufSize(307200)
{
   #ifdef WIN32
      WORD wVersionRequested;
      WSADATA wsaData;
      wVersionRequested = MAKEWORD(2, 2);

      if (0 != WSAStartup(wVersionRequested, &wsaData))
         throw CUDTException(1, 0, NET_ERROR);
   #endif

   m_pcChannelBuf = new char [9000];
}

CChannel::CChannel(const __int32& version):
m_iIPversion(version),
m_iSndBufSize(102400),
m_iRcvBufSize(307200)
{
   #ifdef WIN32
      WORD wVersionRequested;
      WSADATA wsaData;
      wVersionRequested = MAKEWORD(2, 2);

      if (0 != WSAStartup(wVersionRequested, &wsaData))
         throw CUDTException(1, 0, NET_ERROR);
   #endif

   m_pcChannelBuf = new char [9000];
}

CChannel::~CChannel()
{
   #ifdef WIN32
      WSACleanup();
   #endif

   delete [] m_pcChannelBuf;
}

void CChannel::open(const sockaddr* addr)
{
   // construct an socket
   if (4 == m_iIPversion)
      m_iSocket = socket(AF_INET, SOCK_DGRAM, 0);
   else
      m_iSocket = socket(AF_INET6, SOCK_DGRAM, 0);

   if (m_iSocket < 0)
      throw CUDTException(1, 0, NET_ERROR);

   if (NULL != addr)
   {
      socklen_t namelen = (4 == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

      if (0 != bind(m_iSocket, addr, namelen))
         throw CUDTException(1, 1, NET_ERROR);
   }

   try
   {
      setChannelOpt();
   }
   catch (CUDTException e)
   {
      throw e;
   }
}

void CChannel::disconnect() const
{
   #ifndef WIN32
      close(m_iSocket);
   #else
      closesocket(m_iSocket);
   #endif
}

void CChannel::connect(const sockaddr* addr)
{
   if (0 != ::connect(m_iSocket, addr, sizeof(sockaddr)))
      throw CUDTException(1, 4, NET_ERROR);
}

__int32 CChannel::send(char* buffer, const __int32& size) const
{
   return ::send(m_iSocket, buffer, size, 0);
}

__int32 CChannel::recv(char* buffer, const __int32& size) const
{
   return ::recv(m_iSocket, buffer, size, 0);
}

__int32 CChannel::peek(char* buffer, const __int32& size) const
{
   return ::recv(m_iSocket, buffer, size, MSG_PEEK);
}

const CChannel& CChannel::operator<<(CPacket& packet) const
{
   // convert control information into network order
   if (packet.getFlag())
      for (__int32 i = 0, n = packet.getLength() / sizeof(__int32); i < n; ++ i)
         *((__int32 *)packet.m_pcData + i) = htonl(*((__int32 *)packet.m_pcData + i));

   // convert packet header into network order
   packet.m_nHeader = htonl(packet.m_nHeader);

   #ifdef UNIX
      while (0 == writev(m_iSocket, packet.getPacketVector(), 2)) {}
   #else
      writev(m_iSocket, packet.getPacketVector(), 2);
   #endif

   // convert back into local host order
   packet.m_nHeader = ntohl(packet.m_nHeader);
   if (packet.getFlag())
      for (__int32 i = 0, n = packet.getLength() / sizeof(__int32); i < n; ++ i)
         *((__int32 *)packet.m_pcData + i) = ntohl(*((__int32 *)packet.m_pcData + i));

   return *this;
}

const CChannel& CChannel::operator>>(CPacket& packet) const
{
   // Packet length indicates if the packet is successfully received
   packet.setLength(readv(m_iSocket, packet.getPacketVector(), 2) - sizeof(__int32));

   #ifdef UNIX
      //simulating RCV_TIMEO
      if (packet.getLength() <= 0)
      {
         usleep(10);
         packet.setLength(readv(m_iSocket, packet.getPacketVector(), 2) - sizeof(__int32));
      }
   #endif

   if (packet.getLength() <= 0)
      return *this;

   // convert packet header into local host order
   packet.m_nHeader = ntohl(packet.m_nHeader);

   // convert control information into local host order
   if (packet.getFlag())
      for (__int32 i = 0, n = packet.getLength() / sizeof(__int32); i < n; ++ i)
         *((__int32 *)packet.m_pcData + i) = ntohl(*((__int32 *)packet.m_pcData + i));

   return *this;
}

__int32 CChannel::sendto(CPacket& packet, const sockaddr* addr) const
{
   // convert control information into network order
   if (packet.getFlag())
      for (__int32 i = 0, n = packet.getLength() / sizeof(__int32); i < n; ++ i)
         *((__int32 *)packet.m_pcData + i) = htonl(*((__int32 *)packet.m_pcData + i));

   // convert packet header into network order
   packet.m_nHeader = htonl(packet.m_nHeader);

   char* buf;
   if (sizeof(__int32) + packet.getLength() <= 9000)
      buf = m_pcChannelBuf;
   else
      buf = new char [sizeof(__int32) + packet.getLength()];

   memcpy(buf, packet.getPacketVector()[0].iov_base, sizeof(__int32));
   memcpy(buf + sizeof(__int32), packet.getPacketVector()[1].iov_base, packet.getLength());

   socklen_t addrsize = (4 == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   int ret = ::sendto(m_iSocket, buf, sizeof(__int32) + packet.getLength(), 0, addr, addrsize);

   #ifdef UNIX
      while (ret <= 0)
         ret = ::sendto(m_iSocket, buf, sizeof(__int32) + packet.getLength(), 0, addr, addrsize);
   #endif

   if (sizeof(__int32) + packet.getLength() > 9000)
      delete [] buf;

   // convert back into local host order
   packet.m_nHeader = ntohl(packet.m_nHeader);
   if (packet.getFlag())
      for (__int32 i = 0, n = packet.getLength() / sizeof(__int32); i < n; ++ i)
         *((__int32 *)packet.m_pcData + i) = ntohl(*((__int32 *)packet.m_pcData + i));

   return ret;
}

__int32 CChannel::recvfrom(CPacket& packet, sockaddr* addr) const
{
   char* buf;
   if (sizeof(__int32) + packet.getLength() <= 9000)
      buf = m_pcChannelBuf;
   else
      buf = new char [sizeof(__int32) + packet.getLength()];

   socklen_t addrsize = (4 == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   int ret = ::recvfrom(m_iSocket, buf, sizeof(__int32) + packet.getLength(), 0, addr, &addrsize);

   #ifdef UNIX
      //simulating RCV_TIMEO
      if (ret <= 0)
      {
         usleep(10);
         ret = ::recvfrom(m_iSocket, buf, sizeof(__int32) + packet.getLength(), 0, addr, &addrsize);
      }
   #endif

   if (ret > int(sizeof(__int32)))
   {
      packet.setLength(ret - sizeof(__int32));
      memcpy(packet.getPacketVector()[0].iov_base, buf, sizeof(__int32));
      memcpy(packet.getPacketVector()[1].iov_base, buf + sizeof(__int32), ret - sizeof(__int32));

      // convert back into local host order
      packet.m_nHeader = ntohl(packet.m_nHeader);
      if (packet.getFlag())
         for (__int32 i = 0, n = packet.getLength() / sizeof(__int32); i < n; ++ i)
            *((__int32 *)packet.m_pcData + i) = ntohl(*((__int32 *)packet.m_pcData + i));
   }
   else
   {
      if (ret > 0)
         ret = 0;
      packet.setLength(ret);
   }

   if (sizeof(__int32) + packet.getLength() > 9000)
      delete [] buf;

   return ret;
}

__int32 CChannel::getSndBufSize()
{
   socklen_t size;

   getsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, &size);

   return m_iSndBufSize;
}

__int32 CChannel::getRcvBufSize()
{
   socklen_t size;

   getsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, &size);

   return m_iRcvBufSize;
}

void CChannel::setSndBufSize(const __int32& size)
{
   m_iSndBufSize = size;
}

void CChannel::setRcvBufSize(const __int32& size)
{
   m_iRcvBufSize = size;
}

void CChannel::getSockAddr(sockaddr* addr) const
{
   socklen_t namelen;

   if (4 == m_iIPversion)
      namelen = sizeof(sockaddr_in);
   else
      namelen = sizeof(sockaddr_in6);

   getsockname(m_iSocket, addr, &namelen);
}

void CChannel::getPeerAddr(sockaddr* addr) const
{
   socklen_t namelen;

   if (4 == m_iIPversion)
      namelen = sizeof(sockaddr_in);
   else
      namelen = sizeof(sockaddr_in6);

   getpeername(m_iSocket, addr, &namelen);
}

void CChannel::setChannelOpt()
{
   // set sending and receiving buffer size
   if ((0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, sizeof(__int32))) ||
       (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, sizeof(__int32))))
      throw CUDTException(1, 2, NET_ERROR);

   timeval tv;
   tv.tv_sec = 0;
   #ifdef BSD
      // Known BSD bug as the day I wrote these codes.
      // A small time out value will cause the socket to block forever.
      tv.tv_usec = 10000;
   #else
      tv.tv_usec = 100;
   #endif

   #ifdef UNIX
      // Set non-blocking I/O
      // UNIX does not support SO_RCVTIMEO
      __int32 opts = fcntl(m_iSocket, F_GETFL);
      if (-1 == fcntl(m_iSocket, F_SETFL, opts | O_NONBLOCK))
         throw CUDTException(1, 2, NET_ERROR);
   #elif WIN32
      DWORD ot = 1; //milliseconds
      if (setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&ot, sizeof(DWORD)) < 0)
         throw CUDTException(1, 2, NET_ERROR);
   #else
      // Set receiving time-out value
      if (setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(timeval)) < 0)
         throw CUDTException(1, 2, NET_ERROR);
   #endif
}

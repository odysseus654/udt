/*****************************************************************************
Copyright � 2001 - 2005, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) version 2

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
****************************************************************************/

/*****************************************************************************
This file contains the implementation of UDT packet sending and receiving
routines.

UDT uses UDP for packet transfer. Data gathering/scattering is used in
both sending and receiving.

reference:
socket programming reference, writev/readv
UDT packet definition: packet.h
*****************************************************************************/

/****************************************************************************
written by
   Yunhong Gu [ygu@cs.uic.edu], last updated 02/16/2005

modified by
   <programmer's name, programmer's email, last updated mm/dd/yyyy>
   <descrition of changes>
*****************************************************************************/


#ifndef WIN32
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <unistd.h>
   #include <fcntl.h>
   #include <cstring>
   #include <cstdio>
   #include <cerrno>
   #include <dlfcn.h>
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

   #ifdef CAPI
      m_pHSysSockLib = dlopen(g_pcSysLibPath, RTLD_NOW);
      if (NULL == m_pHSysSockLib)
         throw CUDTException(1, 0, errno);

      sys_socket = (int(*)(int, int, int))dlsym(m_pHSysSockLib, "socket");
      sys_bind = (int(*)(int, const struct sockaddr*, unsigned int))dlsym(m_pHSysSockLib, "bind");
      sys_connect = (int(*)(int, const struct sockaddr*, socklen_t))dlsym(m_pHSysSockLib, "connect");
      sys_close = (int(*)(int))dlsym(m_pHSysSockLib, "close");
      sys_send = (ssize_t(*)(int, const void*, unsigned int, int))dlsym(m_pHSysSockLib, "send");
      sys_recv = (ssize_t(*)(int, void*, unsigned int, int))dlsym(m_pHSysSockLib, "recv");
      sys_getpeername = (int(*)(int, struct sockaddr*, socklen_t*))dlsym(m_pHSysSockLib, "getpeername");
      sys_getsockname = (int(*)(int, struct sockaddr*, socklen_t*))dlsym(m_pHSysSockLib, "getsockname");
      sys_getsockopt = (int(*)(int, int, int, void*, socklen_t*))dlsym(m_pHSysSockLib, "getsockopt");
      sys_setsockopt = (int(*)(int, int, int, const void*, socklen_t))dlsym(m_pHSysSockLib, "setsockopt");
   #endif

   m_pcChannelBuf = new char [9000];
}

CChannel::CChannel(const __int32& version):
m_iIPversion(version),
m_iSndBufSize(102400),
m_iRcvBufSize(409600)
{
   #ifdef WIN32
      WORD wVersionRequested;
      WSADATA wsaData;
      wVersionRequested = MAKEWORD(2, 2);

      if (0 != WSAStartup(wVersionRequested, &wsaData))
         throw CUDTException(1, 0, NET_ERROR);
   #endif

   #ifdef CAPI
      m_pHSysSockLib = dlopen(g_pcSysLibPath, RTLD_NOW);
      if (NULL == m_pHSysSockLib)
         throw CUDTException(1, 0, errno);

      sys_socket = (int(*)(int, int, int))dlsym(m_pHSysSockLib, "socket");
      sys_bind = (int(*)(int, const struct sockaddr*, unsigned int))dlsym(m_pHSysSockLib, "bind");
      sys_connect = (int(*)(int, const struct sockaddr*, socklen_t))dlsym(m_pHSysSockLib, "connect");
      sys_close = (int(*)(int))dlsym(m_pHSysSockLib, "close");
      sys_send = (ssize_t(*)(int, const void*, unsigned int, int))dlsym(m_pHSysSockLib, "send");
      sys_recv = (ssize_t(*)(int, void*, unsigned int, int))dlsym(m_pHSysSockLib, "recv");
      sys_getpeername = (int(*)(int, struct sockaddr*, socklen_t*))dlsym(m_pHSysSockLib, "getpeername");
      sys_getsockname = (int(*)(int, struct sockaddr*, socklen_t*))dlsym(m_pHSysSockLib, "getsockname");
      sys_getsockopt = (int(*)(int, int, int, void*, socklen_t*))dlsym(m_pHSysSockLib, "getsockopt");
      sys_setsockopt = (int(*)(int, int, int, const void*, socklen_t))dlsym(m_pHSysSockLib, "setsockopt");
   #endif

   m_pcChannelBuf = new char [9000];
}

CChannel::~CChannel()
{
   #ifdef WIN32
      WSACleanup();
   #endif

   #ifdef CAPI
      dlclose(m_pHSysSockLib);
   #endif

   delete [] m_pcChannelBuf;
}

void CChannel::open(const sockaddr* addr)
{
   // construct an socket
   #ifndef CAPI
      if (4 == m_iIPversion)
         m_iSocket = socket(AF_INET, SOCK_DGRAM, 0);
      else
         m_iSocket = socket(AF_INET6, SOCK_DGRAM, 0);
   #else
      if (4 == m_iIPversion)
         m_iSocket = (*sys_socket)(AF_INET, SOCK_DGRAM, 0);
      else
         m_iSocket = (*sys_socket)(AF_INET6, SOCK_DGRAM, 0);
   #endif

   if (m_iSocket < 0)
      throw CUDTException(1, 0, NET_ERROR);

   if (NULL != addr)
   {
      socklen_t namelen = (4 == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

      #ifndef CAPI
         if (0 != bind(m_iSocket, addr, namelen))
            throw CUDTException(1, 1, NET_ERROR);
      #else
         if (0 != (*sys_bind)(m_iSocket, addr, namelen))
            throw CUDTException(1, 1, NET_ERROR);
      #endif
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
      #ifndef CAPI
         close(m_iSocket);
      #else
         (*sys_close)(m_iSocket);
      #endif
   #else
      closesocket(m_iSocket);
   #endif
}

void CChannel::connect(const sockaddr* addr)
{
   #ifndef CAPI
      if (0 != ::connect(m_iSocket, addr, sizeof(sockaddr)))
         throw CUDTException(1, 4, NET_ERROR);
   #else
      if (0 != (*sys_connect)(m_iSocket, addr, sizeof(sockaddr)))
         throw CUDTException(1, 4, NET_ERROR);
   #endif
}

__int32 CChannel::send(char* buffer, const __int32& size) const
{
   #ifndef CAPI
      return ::send(m_iSocket, buffer, size, 0);
   #else
      return (*sys_send)(m_iSocket, buffer, size, 0);
   #endif
}

__int32 CChannel::recv(char* buffer, const __int32& size) const
{
   #ifndef CAPI
      return ::recv(m_iSocket, buffer, size, 0);
   #else
      return (*sys_recv)(m_iSocket, buffer, size, 0);
   #endif
}

__int32 CChannel::peek(char* buffer, const __int32& size) const
{
   #ifndef CAPI
      return ::recv(m_iSocket, buffer, size, MSG_PEEK);
   #else
      return (*sys_recv)(m_iSocket, buffer, size, MSG_PEEK);
   #endif
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
   packet.setLength(readv(m_iSocket, packet.getPacketVector(), 2) - CPacket::m_iPktHdrSize);

   #ifdef UNIX
      //simulating RCV_TIMEO
      if (packet.getLength() <= 0)
      {
         usleep(10);
         packet.setLength(readv(m_iSocket, packet.getPacketVector(), 2) - CPacket::m_iPktHdrSize);
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
   if (CPacket::m_iPktHdrSize + packet.getLength() <= 9000)
      buf = m_pcChannelBuf;
   else
      buf = new char [CPacket::m_iPktHdrSize + packet.getLength()];

   memcpy(buf, packet.getPacketVector()[0].iov_base, CPacket::m_iPktHdrSize);
   memcpy(buf + CPacket::m_iPktHdrSize, packet.getPacketVector()[1].iov_base, packet.getLength());

   socklen_t addrsize = (4 == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   int ret = ::sendto(m_iSocket, buf, CPacket::m_iPktHdrSize + packet.getLength(), 0, addr, addrsize);

   #ifdef UNIX
      while (ret <= 0)
         ret = ::sendto(m_iSocket, buf, CPacket::m_iPktHdrSize + packet.getLength(), 0, addr, addrsize);
   #endif

   if (CPacket::m_iPktHdrSize + packet.getLength() > 9000)
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
   if (CPacket::m_iPktHdrSize + packet.getLength() <= 9000)
      buf = m_pcChannelBuf;
   else
      buf = new char [CPacket::m_iPktHdrSize + packet.getLength()];

   socklen_t addrsize = (4 == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   int ret = ::recvfrom(m_iSocket, buf, CPacket::m_iPktHdrSize + packet.getLength(), 0, addr, &addrsize);

   #ifdef UNIX
      //simulating RCV_TIMEO
      if (ret <= 0)
      {
         usleep(10);
         ret = ::recvfrom(m_iSocket, buf, CPacket::m_iPktHdrSize + packet.getLength(), 0, addr, &addrsize);
      }
   #endif

   if (ret > CPacket::m_iPktHdrSize)
   {
      packet.setLength(ret - CPacket::m_iPktHdrSize);
      memcpy(packet.getPacketVector()[0].iov_base, buf, CPacket::m_iPktHdrSize);
      memcpy(packet.getPacketVector()[1].iov_base, buf + CPacket::m_iPktHdrSize, ret - CPacket::m_iPktHdrSize);

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

   if (CPacket::m_iPktHdrSize + packet.getLength() > 9000)
      delete [] buf;

   return ret;
}

__int32 CChannel::getSndBufSize()
{
   socklen_t size;

   #ifndef CAPI
      getsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, &size);
   #else
      (*sys_getsockopt)(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, &size);
   #endif

   return m_iSndBufSize;
}

__int32 CChannel::getRcvBufSize()
{
   socklen_t size;

   #ifndef CAPI
      getsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, &size);
   #else
      (*sys_getsockopt)(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, &size);
   #endif

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

   #ifndef CAPI
      getsockname(m_iSocket, addr, &namelen);
   #else
      (*sys_getsockname)(m_iSocket, addr, &namelen);
   #endif
}

void CChannel::getPeerAddr(sockaddr* addr) const
{
   socklen_t namelen;

   if (4 == m_iIPversion)
      namelen = sizeof(sockaddr_in);
   else
      namelen = sizeof(sockaddr_in6);

   #ifndef CAPI
      getpeername(m_iSocket, addr, &namelen);
   #else
      (*sys_getpeername)(m_iSocket, addr, &namelen);
   #endif
}

void CChannel::setChannelOpt()
{
   // set sending and receiving buffer size
   #ifndef CAPI
      if ((0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, sizeof(__int32))) ||
          (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, sizeof(__int32))))
         throw CUDTException(1, 2, NET_ERROR);
   #else
      if ((0 != (*sys_setsockopt)(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, sizeof(__int32))) ||
          (0 != (*sys_setsockopt)(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, sizeof(__int32))))
         throw CUDTException(1, 2, NET_ERROR);
   #endif

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
      #ifndef CAPI
         if (setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(timeval)) < 0)
            throw CUDTException(1, 2, NET_ERROR);
      #else
         if ((*sys_setsockopt)(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(timeval)) < 0)
            throw CUDTException(1, 2, NET_ERROR);
      #endif
   #endif
}

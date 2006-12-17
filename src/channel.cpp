/*****************************************************************************
Copyright � 2001 - 2006, The Board of Trustees of the University of Illinois.
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
   Yunhong Gu [gu@lac.uic.edu], last updated 12/12/2006
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
#include "channel.h"
#include "packet.h"
#include <iostream>

using namespace std;

#ifdef WIN32
   #define socklen_t int
#endif

#ifndef WIN32
   #define NET_ERROR errno
#else
   #define NET_ERROR WSAGetLastError()
#endif


CChannel::CChannel():
m_iIPversion(AF_INET),
m_iSndBufSize(65536),
m_iRcvBufSize(65536)
{
}

CChannel::CChannel(const int& version):
m_iIPversion(version),
m_iSndBufSize(65536),
m_iRcvBufSize(65536)
{
}

CChannel::~CChannel()
{
}

void CChannel::open(const sockaddr* addr)
{
   // construct an socket
   m_iSocket = socket(m_iIPversion, SOCK_DGRAM, 0);

   if (m_iSocket < 0)
      throw CUDTException(1, 0, NET_ERROR);

   if (NULL != addr)
   {
      socklen_t namelen = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

      if (0 != bind(m_iSocket, addr, namelen))
         throw CUDTException(1, 3, NET_ERROR);
   }
   else
   {
      //sendto or WSASendTo will also automatically bind the socket
      addrinfo hints;
      addrinfo* res;

      memset(&hints, 0, sizeof(struct addrinfo));

      hints.ai_flags = AI_PASSIVE;
      hints.ai_family = m_iIPversion;
      hints.ai_socktype = SOCK_DGRAM;

      char* service = "0";

      if (0 != getaddrinfo(NULL, service, &hints, &res))
		  throw CUDTException(1, 3, NET_ERROR);

      if (0 != bind(m_iSocket, res->ai_addr, res->ai_addrlen))
		  throw CUDTException(1, 3, NET_ERROR);

      freeaddrinfo(res);
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

void CChannel::close() const
{
   #ifndef WIN32
      ::close(m_iSocket);
   #else
      closesocket(m_iSocket);
   #endif
}

int CChannel::getSndBufSize()
{
   socklen_t size = sizeof(socklen_t);

   getsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, &size);

   return m_iSndBufSize;
}

int CChannel::getRcvBufSize()
{
   socklen_t size = sizeof(socklen_t);

   getsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, &size);

   return m_iRcvBufSize;
}

void CChannel::setSndBufSize(const int& size)
{
   m_iSndBufSize = size;
}

void CChannel::setRcvBufSize(const int& size)
{
   m_iRcvBufSize = size;
}

void CChannel::getSockAddr(sockaddr* addr) const
{
   socklen_t namelen = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   getsockname(m_iSocket, addr, &namelen);
}

void CChannel::getPeerAddr(sockaddr* addr) const
{
   socklen_t namelen = (AF_INET == m_iIPversion) ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);

   getpeername(m_iSocket, addr, &namelen);
}

void CChannel::setChannelOpt()
{
   // set sending and receiving buffer size
   if ((0 != setsockopt(m_iSocket, SOL_SOCKET, SO_RCVBUF, (char *)&m_iRcvBufSize, sizeof(int))) ||
       (0 != setsockopt(m_iSocket, SOL_SOCKET, SO_SNDBUF, (char *)&m_iSndBufSize, sizeof(int))))
      throw CUDTException(1, 3, NET_ERROR);

/*
   timeval tv;
   tv.tv_sec = 0;
   #if defined (BSD) || defined (OSX)
      // Known BSD bug as the day I wrote these codes.
      // A small time out value will cause the socket to block forever.
      tv.tv_usec = 10000;
   #else
      tv.tv_usec = 100;
   #endif

   #ifdef UNIX
      // Set non-blocking I/O
      // UNIX does not support SO_RCVTIMEO
      int opts = fcntl(m_iSocket, F_GETFL);
      if (-1 == fcntl(m_iSocket, F_SETFL, opts | O_NONBLOCK))
         throw CUDTException(1, 3, NET_ERROR);
   #elif WIN32
      DWORD ot = 1; //milliseconds
      if (setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&ot, sizeof(DWORD)) < 0)
         throw CUDTException(1, 3, NET_ERROR);
   #else
      // Set receiving time-out value
      if (setsockopt(m_iSocket, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(timeval)) < 0)
         throw CUDTException(1, 3, NET_ERROR);
   #endif
*/
}

int CChannel::sendto(const sockaddr* addr, CPacket& packet) const
{
   // convert control information into network order
   if (packet.getFlag())
      for (int i = 0, n = packet.getLength() / 4; i < n; ++ i)
         *((uint32_t *)packet.m_pcData + i) = htonl(*((uint32_t *)packet.m_pcData + i));

   // convert packet header into network order
   for (int j = 0; j < 4; ++ j)
      packet.m_nHeader[j] = htonl(packet.m_nHeader[j]);

   #ifndef WIN32
      msghdr mh;
      mh.msg_name = (sockaddr*)addr;
      mh.msg_namelen = sizeof(sockaddr_in);
      mh.msg_iov = (iovec*)packet.m_PacketVector;
      mh.msg_iovlen = 2;
      mh.msg_control = NULL;
      mh.msg_controllen = 0;
      mh.msg_flags = 0;

      int res = sendmsg(m_iSocket, &mh, 0);
   #else
      DWORD size = CPacket::m_iPktHdrSize + packet.getLength();
      int res = WSASendTo(m_iSocket, (LPWSABUF)packet.m_PacketVector, 2, &size, 0, addr, sizeof(sockaddr_in), NULL, NULL);
      res = (0 == res) ? size : -1;
   #endif

   // convert back into local host order
   for (int k = 0; k < 4; ++ k)
      packet.m_nHeader[k] = ntohl(packet.m_nHeader[k]);

   if (packet.getFlag())
      for (int l = 0, n = packet.getLength() / 4; l < n; ++ l)
         *((uint32_t *)packet.m_pcData + l) = ntohl(*((uint32_t *)packet.m_pcData + l));

   return res;
}

int CChannel::recvfrom(sockaddr* addr, CPacket& packet) const
{
   #ifndef WIN32
      msghdr mh;   
      mh.msg_name = addr;
      mh.msg_namelen = sizeof(sockaddr_in);
      mh.msg_iov = packet.m_PacketVector;
      mh.msg_iovlen = 2;
      mh.msg_control = NULL;
      mh.msg_controllen = 0;
      mh.msg_flags = 0;

      int res = recvmsg(m_iSocket, &mh, 0);
   #else
      DWORD size = CPacket::m_iPktHdrSize + packet.getLength();
      DWORD flag = 0;
      int addrsize = sizeof(sockaddr_in);

      int res = WSARecvFrom(m_iSocket, (LPWSABUF)packet.m_PacketVector, 2, &size, &flag, addr, &addrsize, NULL, NULL);
      res = (0 == res) ? size : -1;
   #endif

   if (res <= 0)
   {
      packet.setLength(-1);
      return -1;
   }

   packet.setLength(res - CPacket::m_iPktHdrSize);

   // convert back into local host order
   for (int i = 0; i < 4; ++ i)
      packet.m_nHeader[i] = ntohl(packet.m_nHeader[i]);

   if (packet.getFlag())
      for (int j = 0, n = packet.getLength() / 4; j < n; ++ j)
         *((uint32_t *)packet.m_pcData + j) = ntohl(*((uint32_t *)packet.m_pcData + j));

   return packet.getLength();
}

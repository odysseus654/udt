/*****************************************************************************
Copyright © 2001 - 2005, The Board of Trustees of the University of Illinois.
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
*****************************************************************************/

/*****************************************************************************
UDT "C" socket API
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [ygu@cs.uic.edu], last updated 02/16/2005

modified by
   <programmer's name, programmer's email, last updated mm/dd/yyyy>
   <descrition of changes>
*****************************************************************************/

#include "udt.h"

#ifdef CAPI

extern "C" 
{

int socket(int af, int type, int flag)
{
   if (SOCK_STREAM == type)
      return CUDT::socket(af, type, flag);

   return -1;
}

int bind(int u, const struct sockaddr* name, unsigned int namelen)
{
//   if u  in UDT
//     do
//   else
//     use sys call

   return CUDT::bind(u, name, namelen);
}

int listen(int u, int backlog)
{
   return CUDT::listen(u, backlog);
}

int accept(int u, struct sockaddr* addr, socklen_t* addrlen)
{
   return CUDT::accept(u, addr, (int*)addrlen);
}

int connect(int u, const struct sockaddr* name, socklen_t namelen)
{
   return CUDT::connect(u, name, namelen);
}

int close(int u)
{
   return CUDT::close(u);
}

int getpeername(int u, struct sockaddr* name, socklen_t* namelen)
{
   return CUDT::getpeername(u, name, (int*)namelen);
}

int getsockname(int u, struct sockaddr* name, socklen_t* namelen)
{
   return CUDT::getsockname(u, name, (int*)namelen);
}

int getsockopt(int u, int level, int optname, void* optval, socklen_t* optlen)
{
   switch (optname)
   {
   case SO_SNDBUF:
      return CUDT::getsockopt(u, level, UDT_SNDBUF, optval, (int*)optlen);

   case SO_RCVBUF:
      return CUDT::getsockopt(u, level, UDT_RCVBUF, optval, (int*)optlen);

   case SO_LINGER:
      return CUDT::getsockopt(u, level, UDT_LINGER, optval, (int*)optlen);

   default:
      return 0;
   }
}

int setsockopt(int u, int level, int optname, const void* optval, socklen_t optlen)
{
   switch (optname)
   {
   case SO_SNDBUF:
      return CUDT::setsockopt(u, level, UDT_SNDBUF, optval, optlen);

   case SO_RCVBUF:
      return CUDT::setsockopt(u, level, UDT_RCVBUF, optval, optlen);

   case SO_LINGER:
      return CUDT::setsockopt(u, level, UDT_LINGER, optval, optlen);

   default:
      return 0;
   }
}

//fcntl
// use dlopen!

int shutdown(int u, int how)
{
   return CUDT::shutdown(u, how);
}

ssize_t send(int u, const void* buf, unsigned int len, int flags)
{
   return CUDT::send(u, (char*)buf, len, flags, NULL, NULL);
}

ssize_t recv(int u, void* buf, unsigned int len, int flags)
{
   return CUDT::recv(u, (char*)buf, len, flags, NULL, NULL);
}

ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
//long long sendfile(int u, char* filename, long long offset, long long size, int block)
{
   return -1;

//   ifstream ifs(filename);
//   return CUDT::sendfile(out_fd, ifs, *offset, count, 367000);
}

int select(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
//int select(int nfds, ud_set* readfds, ud_set* writefds, ud_set* exceptfds, struct timeval* timeout)
{
   return -1;
   //return CUDT::select(n, readfds, writefds, exceptfds, timeout);

// dlopen
// system select???
}

}

#endif

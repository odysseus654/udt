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
channel.h
Header file for data and control connections

Author: Yunhong Gu [gu@lac.uic.edu]
Last Update: Oct. 08, 2002
********************************************************************/


#ifndef _SABUL_CHANNEL_H_
#define _SABUL_CHANNEL_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include "packet.h"

class CChannel
{
public:
   CChannel();
   virtual ~CChannel();

   virtual int send(char* buffer, const int& size) const {return 0;}
   virtual int recv(char* buffer, const int& size) const {return 0;}
   virtual int peek(char* buffer, const int& size) const {return 0;}

   // connect to the other side
   virtual void connect(const char* ip, const int& port) {}

   // waiting to be connected
   virtual const sockaddr_in& connect() {return m_RemoteSockAddr;}

   virtual void disconnect() const;

protected:
   int m_iSocket;
   sockaddr_in m_SockAddr;
   sockaddr_in m_RemoteSockAddr;

   const int m_iBacklog;
   const unsigned int m_uiSendBufSize;
   const unsigned int m_uiRecvBufSize;
};

class CUdpChannel: public CChannel
{
public:
   CUdpChannel();
   CUdpChannel(int& port);
   //For hosts with multiple IP, UDP can be also bound to an assigned IP address
   CUdpChannel(const char* ip, int& port);
   virtual ~CUdpChannel();

   // overloads of sequencial IO operaters
   CUdpChannel& operator<<(CPktSblData& sdpkt) const;
   CUdpChannel& operator>>(CPktSblData& sdpkt) const;
   CUdpChannel& operator<<(CPacketVector& sdpkt) const;
   CUdpChannel& operator>>(CPacketVector& sdpkt) const;

   int send(char* buffer, const int& size) const;
   int recv(char* buffer, const int& size) const;
   int peek(char* buffer, const int& size) const;

   void connect(const char* ip, const int& port);
   const sockaddr_in& connect();
};

class CTcpChannel: public CChannel
{
public:
   CTcpChannel();
   CTcpChannel(int& port);
   //For hosts with multiple IP, TCP can be also bound to an assigned IP address
   CTcpChannel(const char* ip, int& port);
   virtual ~CTcpChannel();

   // overloads of sequencial IO operators
   CTcpChannel& operator<<(CPktSblCtrl& scpkt) const;
   CTcpChannel& operator>>(CPktSblCtrl& scpkt) const;

   int send(char* buffer, const int& size) const;
   int recv(char* buffer, const int& size) const;

   void connect(const char* ip, const int& port);
   const sockaddr_in& connect();

private:
   pollfd* m_pPollFD;

   const long m_lTimerOut;
};

#endif

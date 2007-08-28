/*****************************************************************************
Copyright � 2001 - 2007, The Board of Trustees of the University of Illinois.
All Rights Reserved.

UDP-based Data Transfer Library (UDT) version 4

National Center for Data Mining (NCDM)
University of Illinois at Chicago
http://www.ncdm.uic.edu/

UDT is free software; you can redistribute it and/or modify it under the
terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option)
any later version.

UDT is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

/*****************************************************************************
This header file contains the definition of Window structures used in UDT.
*****************************************************************************/

/*****************************************************************************
written by
   Yunhong Gu [gu@lac.uic.edu], last updated 08/22/2007
*****************************************************************************/

#ifndef __UDT_WINDOW_H__
#define __UDT_WINDOW_H__


#ifndef WIN32
   #include <sys/time.h>
   #include <time.h>
#endif
#include "udt.h"


class CACKWindow
{
public:
   CACKWindow();
   CACKWindow(const int& size);
   ~CACKWindow();

      // Functionality:
      //    Write an ACK record into the window.
      // Parameters:
      //    0) [in] seq: ACK seq. no.
      //    1) [in] ack: DATA ACK no.
      // Returned value:
      //    None.

   void store(const int32_t& seq, const int32_t& ack);

      // Functionality:
      //    Search the ACK-2 "seq" in the window, find out the DATA "ack" and caluclate RTT .
      // Parameters:
      //    0) [in] seq: ACK-2 seq. no.
      //    1) [out] ack: the DATA ACK no. that matches the ACK-2 no.
      // Returned value:
      //    RTT.

   int acknowledge(const int32_t& seq, int32_t& ack);

private:
   int32_t* m_piACKSeqNo;       // Seq. No. for the ACK packet
   int32_t* m_piACK;            // Data Seq. No. carried by the ACK packet
   uint64_t* m_pTimeStamp;      // The timestamp when the ACK was sent

   int m_iSize;                 // Size of the ACK history window
   int m_iHead;                 // Pointer to the lastest ACK record
   int m_iTail;                 // Pointer to the oldest ACK record
};

////////////////////////////////////////////////////////////////////////////////

class CPktTimeWindow
{
public:
   CPktTimeWindow();
   CPktTimeWindow(const int& asize, const int& psize);
   ~CPktTimeWindow();

      // Functionality:
      //    read the minimum packet sending interval.
      // Parameters:
      //    None.
      // Returned value:
      //    minimum packet sending interval (microseconds).

   int getMinPktSndInt() const;

      // Functionality:
      //    Calculate the packes arrival speed.
      // Parameters:
      //    None.
      // Returned value:
      //    Packet arrival speed (packets per second).

   int getPktRcvSpeed() const;

      // Functionality:
      //    Estimate the bandwidth.
      // Parameters:
      //    None.
      // Returned value:
      //    Estimated bandwidth (packets per second).

   int getBandwidth() const;

      // Functionality:
      //    Record time information of a packet sending.
      // Parameters:
      //    0) currtime: timestamp of the packet sending.
      // Returned value:
      //    None.

   void onPktSent(const int& currtime);

      // Functionality:
      //    Record time information of an arrived packet.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void onPktArrival();

      // Functionality:
      //    Record the arrival time of the first probing packet.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void probe1Arrival();

      // Functionality:
      //    Record the arrival time of the second probing packet and the interval between packet pairs.
      // Parameters:
      //    None.
      // Returned value:
      //    None.

   void probe2Arrival();

private:
   int m_iAWSize;               // size of the packet arrival history window
   int* m_piPktWindow;          // packet information window
   int m_iPktWindowPtr;         // position pointer of the packet info. window.

   int m_iPWSize;               // size of probe history window size
   int* m_piProbeWindow;        // record inter-packet time for probing packet pairs
   int m_iProbeWindowPtr;       // position pointer to the probing window

   int m_iLastSentTime;         // last packet sending time
   int m_iMinPktSndInt;         // Minimum packet sending interval

   uint64_t m_LastArrTime;      // last packet arrival time
   uint64_t m_CurrArrTime;      // current packet arrival time
   uint64_t m_ProbeTime;        // arrival time of the first probing packet
};


#endif

/***************************************************************************
                          ipaddr.h  -  description
                             -------------------
    begin                : Wed Dec 6 2000
    copyright            : (C) 2000 by Marco Mazzucco
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef IPADDR_H
#define IPADDR_H


/**
  *@author Marco Mazzucco
  */

class SabulIpaddr {
public:

		// General Unix socket address
		struct sockaddr * m_pSA;  // system defined structure
		// IPv4 socket address
		struct sockaddr_in * m_pSA_in; // system defined structure
		
		//This are the human readable form of the socket address
		//StringSabulIpaddr needs to be constant for later calls
		int m_iPortNum;
		const char  *m_pcStringSabulIpaddr;
		char  *m_caStringSocketAddr;
                     

//IP string address to Network Address
	SabulIpaddr(const char* a_pcSockAddr="0.0.0.0.0");
//IP Network address to String Address
	SabulIpaddr(const struct sockaddr *a_pSa);
	~SabulIpaddr();

//Change the udp port number in the ipaddr object.
void ChangePort(int);
  
private:

	char m_cStrbuf[20];
	struct sockaddr_in * sin;
};

#endif

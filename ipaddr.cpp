/***************************************************************************
                          ipaddr.cpp  -  description
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

#include "config.h"

#include "ipaddr.h"
#include<string.h>
#include <unistd.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <stdio.h>    
#include <iostream.h>


//String address to Network address

SabulIpaddr::SabulIpaddr(const char* a_pcStringSockAddr="0.0.0.0.0")
{

	//const char *ConstSockAddr=a_pcStringSockAddr, *ConstStringIP;

	char  c_A[4], c_B[4], c_C[4], c_D[4], c_P[6]; // marco
	u_short s_Port; //jessie
	m_pSA_in = new struct sockaddr_in ;

	sscanf(a_pcStringSockAddr,"%[0-9].%[0-9].%[0-9].%[0-9].%[0-9]",c_A,c_B,c_C,c_D,c_P);

	s_Port=(u_short) atoi(c_P); //jessie
	m_iPortNum=s_Port;

	sprintf(m_cStrbuf,"%s.%s.%s.%s",c_A,c_B,c_C,c_D);
	m_pcStringSabulIpaddr=m_cStrbuf;

	m_pSA_in->sin_port = htons(s_Port);
	m_pSA_in->sin_family= AF_INET;
	m_pSA_in->sin_addr.s_addr = inet_addr(m_pcStringSabulIpaddr);

	m_pSA = (struct sockaddr *) m_pSA_in;

}


//Convert NetIP to StingIP

SabulIpaddr::SabulIpaddr(const struct sockaddr *a_pSa)
{
	char portstr[7];
	sin = (struct sockaddr_in *) a_pSa;

	// change inet_ntoa for there is no inet_ntop in sun
	// if (inet_ntop(PF_INET, &sin->sin_addr, m_caStringSocketAddr , sizeof(m_caStringSocketAddr)) == NULL)
        //    printf("allocation error in SabulIpaddr \n");
        //
 	m_caStringSocketAddr = inet_ntoa(sin->sin_addr);

	if (ntohs(sin->sin_port) != 0) {
	    snprintf(portstr, sizeof(portstr), ".%d", ntohs(sin->sin_port));
	    strcat(m_caStringSocketAddr, portstr); }

  	return;

}

// Change the udp port number
void SabulIpaddr::ChangePort(int a_iPortnum)
{
     this->m_iPortNum=a_iPortnum;
     this->m_pSA_in->sin_port=htons(a_iPortnum);
     sprintf(this->m_caStringSocketAddr,"%s.%d",this->m_pcStringSabulIpaddr,a_iPortnum);  
}

    
SabulIpaddr::~SabulIpaddr()
{
	delete m_pSA;
}

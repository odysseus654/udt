/***************************************************************************
                          main.cpp  -  description
                             -------------------
    begin                : Wed Nov 22 16:46:10 CST 2000
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <iostream.h>
#include <stdlib.h>
#include <unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include <stdio.h>
#include "ipaddr.h"


int main(int argc, char *argv[])
{
  cout << "Hello, World!" << endl;

  IPAddr StrIP("127.0.0.1.123");
  printf("main StringIPAddr %s \n", StrIP.StringIPAddr);
  IPAddr NetIP( StrIP.SA );
  
  cout << NetIP.StringSocketAddr << endl;

  return EXIT_SUCCESS;
}

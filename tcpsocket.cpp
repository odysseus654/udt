/***************************************************************************
                          tcpsocket.cpp  -  description
                             -------------------
    begin                : Tue Nov 28 2000
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
#include "tcpsocket.h"

/**************************************************************************
 *  Method Usage    :       Initiate the data members of the class        *
 *  Method Return   :       None                                          *
 *  Arguments       :       None                                          *
 **************************************************************************/
SabulTcpSocket::SabulTcpSocket()
{
	m_iSid=0;
	m_iCltsid=0;
}

/**************************************************************************
 *  Method Usage    :       Calling the destructors of parent class       *
 *  Method Return   :       None                                          *
 *  Arguments       :       None                                          *
 **************************************************************************/  
SabulTcpSocket::~SabulTcpSocket()
{
}

/**************************************************************************
 *  Method Usage    :       Open a Tcp Socket                             *
 *  Method Return   :       success---socket id; failure--- -1            *
 *  Arguments       :       1. IP address with a port number              *
 *                          2. integer indicating for server(0)/client(1) *
 **************************************************************************/  
int SabulTcpSocket::TcpOpen(const char* a_pcStraddr, int a_iIsClient=0) 
{
	unsigned int iSvalue;
	unsigned int iFvalue;
	SabulIpaddr *pIpaddr = new SabulIpaddr (a_pcStraddr);

    	cout<<pIpaddr->m_pSA_in->sin_port<<endl;  //for test only

    	if(pIpaddr->m_pSA_in->sin_port<=0)
    	{	
        	cout<<"port num wrong"<<pIpaddr->m_iPortNum<<endl;
    	}	
    	else
    	{ 
       		if(a_iIsClient==0) // server
       		{
       			m_iSid=socket(AF_INET, SOCK_STREAM, 0);

       		    	/********** set the socket option to reuseaddr ********************/
       		    	/* 0 success, -1 failure */  
       		    	iSvalue =1;
       		   	if ( setsockopt(m_iSid, SOL_SOCKET, SO_REUSEADDR, &iSvalue, sizeof(unsigned int)) <0 ) 
		   	{
       		        	printf(" Error setting SOL_SOCKET option to 0\n");
               			fflush(stdout);
         		}
           		/***********************************************************************/                                         

           		/* bind----0 success, -1 failure */
           		if(bind(m_iSid,  pIpaddr->m_pSA, sizeof(struct sockaddr_in))<0) 
           		{
               			perror("bind");
	       			return -1;
	   		}
           		/*listen----0 success, -1 failure */
           		if(listen(m_iSid, LISTEN_NUM)<0)
	   		{
               			perror("listen");
	       			return -1;
	   		}
           		struct sockaddr_in *pRemoteAddr = (struct sockaddr_in *)
                                malloc( sizeof( struct sockaddr_in ) );
           		socklen_t temp = sizeof(struct sockaddr_in);
           		memset( pRemoteAddr, 0, temp);       

           		/* Accept connections coming to the server on the specified port */
	   		/* accept---- -1 failure, otherwise, socket descriptor that server process can
	    		* use to communicate with the client exclusively */
           		if((m_iCltsid=accept(m_iSid,(struct sockaddr*)pRemoteAddr, &temp))==-1)
	   		{
               			perror("accept");
	   		}	
           		else  
	   		{
				/********  set sockd NODELAY  *******/
				iSvalue = 1;
                		if ( setsockopt(m_iCltsid, SOL_TCP, TCP_NODELAY, &iSvalue, sizeof(unsigned int) ) < 0 )  
				{
                    			printf( "Error setting TCP_NODELAY option to 0\n" );
                    			fflush( stdout );
                		}
				/************  get socket control  ************/
                                if( (iFvalue = fcntl( m_iCltsid, F_GETFL, 0 ) ) < 0  )
                                {
                                        printf( "Error in getting options\n" );
                                        fflush( stdout );
                                }                                                     
			 	/***********  set socket NONBLOCKING **********/      
				iFvalue |= O_NONBLOCK; 
				if( fcntl( m_iCltsid, F_SETFL, iFvalue) < 0 )  
				{
                    			printf( "Error in setting options\n");
		    			fflush( stdout );
           			}

                		/******* end of setting ***************************/
	   		}
      		}
      		else if(a_iIsClient==1)  //client
      		{
           		m_iSid=socket(AF_INET, SOCK_STREAM, 0);
			/************  get socket control  ************/
                        if( (iFvalue = fcntl( m_iSid, F_GETFL, 0 ) ) < 0  )
                        {
                                printf( "Error in getting options\n" );
                                fflush( stdout );
                        }                                            
           		/********  set sockd BLOCKING *******/                                         
           		iFvalue &= ~O_NONBLOCK;
           		if( fcntl( m_iSid, F_SETFL, iFvalue) < 0 )  
			{
               			printf( "Error in setting options\n");
               			fflush( stdout );
           		}
           		/******* end of setting ***************************/                                          
           		
			/* connect()--- -1 failure, 0 success*/
           		if(connect(m_iSid, pIpaddr->m_pSA, sizeof(struct sockaddr_in))==-1)
               		{
				perror("connect");
			}
      		}
   	} 
    	delete pIpaddr;
    	return m_iSid;
}

/**************************************************************************
 *  Method Usage    :       Sending data using Tcp Socket                 *
 *  Method Return   :       Success---1                                   *
 *  Arguments       :       1. Pointer to the data buffer                 *
 *                          2. Length of the data buffer                  *
 **************************************************************************/  
int SabulTcpSocket::TcpWrite(const void* a_vBuf, int a_iLen ) 
{                                    
   int iLen =0;
   int iRet = 0;
   do
   {
   	if(m_iCltsid==0) //client
   	{
      		iRet = send(m_iSid, ((char *)a_vBuf)+iLen, a_iLen-iLen, 0);
    	}
   	else
	{
      		iRet = send(m_iCltsid,((char *)a_vBuf)+iLen, a_iLen-iLen, 0);
	}
        if(iRet > 0)
	{
		iLen += iRet;
	}
	if(iLen == a_iLen)
	{
		return 1;
	}
    }
    while(true);
}
   
/**************************************************************************
 *  Method Usage    :       Receiving data using Tcp Socket               *
 *  Method Return   :       Success---1                                   *
 *  Arguments       :       1. Pointer to the data buffer                 *
 *                          2. Length of the data buffer                  *
 **************************************************************************/ 
int SabulTcpSocket::TcpRead( void* a_vBuf, int a_iLen)
{
   int iLen=0;
   int iRet=0;

        do
        {
                 if(m_iCltsid==0) //client
                         iRet=recv(m_iSid, ((char *)a_vBuf)+iLen, a_iLen-iLen, 0);
                 else  //server
                         iRet=recv(m_iCltsid, ((char *)a_vBuf)+iLen, a_iLen-iLen, 0);
		 if(iRet >= 0)
		 {
                 	iLen += iRet;
		 }
                 if(iLen == a_iLen)
		 {
                        return 1;
		 }
        }
        while (true);
}

/**************************************************************************
 *  Method Usage    :       Closing Tcp Socket                            *
 *  Method Return   :       Success---1; failure--- -1                    *
 *  Arguments       :       None                                          *
 **************************************************************************/    
int SabulTcpSocket::TcpClose()
{
    shutdown(m_iSid, 2);
    close(m_iSid);
}

           

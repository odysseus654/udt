/************************************************************************

                          hbusockets.cpp  -  description
                             -------------------
    begin                : Fri Nov 24 2000
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
#include "sabul.h"
#include <pthread.h>

pthread_mutex_t mylock1=PTHREAD_MUTEX_INITIALIZER;  // for m_iLostNum
pthread_mutex_t mylock2=PTHREAD_MUTEX_INITIALIZER; // for m_LostList's head                     

/*****************************************************************************************
	This method is a utility which extracts the IP address and port number 	
*****************************************************************************************/
void modStringAddr(char* a_pcAddr, int a_iPort)
{
    	char  A[4], B[4], C[4], D[4], P[6];

/***********
    char* pTemp =new char[24];
    sscanf(a_pcAddr,"%[0-9].%[0-9].%[0-9].%[0-9].%[0-9]",A,B,C,D,P);
    sprintf(pTemp,"%s.%s.%s.%s.%d",A,B,C,D,a_iPort);
    a_pcAddr=pTemp;
    delete pTemp;
*************/
	
	sscanf(a_pcAddr, "%[0-9].%[0-9].%[0-9].%[0-9].%[0-9]",A,B,C,D,P); 
	realloc(a_pcAddr, strlen(a_pcAddr)+3);
	sprintf(a_pcAddr, "%s.%s.%s.%s.%d",A,B,C,D,a_iPort); 

}

/*****************************************************************************************
	This method is just used as a caller during a pthread create 
*****************************************************************************************/
void * caller(void * arg)
{
	((Sabul*)arg)->TcpThread();
	return NULL;	
}

/*****************************************************************************************
		
	Constructor of Sabul used for initializing parameters.

*****************************************************************************************/
Sabul::Sabul()
{
    	m_TcpThreadNum=0;
    	m_iLostNum=0;   
    	m_bLaterRound = false;
    	m_iResendMax = 0;
	m_lSeqNo = 0;
}

/*****************************************************************************************
		
	Destructor of Sabul 

*****************************************************************************************/
Sabul::~Sabul()
{
}

/***************************************************************************************
   
        This method is used for converting connectInfo from host byte order
	to network byte order

 ***************************************************************************************/
void Sabul::htonl_connectInfo(ConnectControl &a_connectInfo) 
{
	a_connectInfo.m_uType = htonl(a_connectInfo.m_uType);
	a_connectInfo.m_uRate = htonl(a_connectInfo.m_uRate);
	a_connectInfo.m_uSockBuffsize = htonl(a_connectInfo.m_uSockBuffsize);
	a_connectInfo.m_uUdpport = htonl(a_connectInfo.m_uUdpport);
 	a_connectInfo.m_uPacksize = htonl(a_connectInfo.m_uPacksize);
}

/***************************************************************************************
  
           This method is used for converting connectInfo from network byte order
           to host byte order
                    
 ***************************************************************************************/          
void Sabul::ntohl_connectInfo(ConnectControl &a_connectInfo)
{
        a_connectInfo.m_uType = ntohl(a_connectInfo.m_uType);
        a_connectInfo.m_uRate = ntohl(a_connectInfo.m_uRate);
        a_connectInfo.m_uSockBuffsize = ntohl(a_connectInfo.m_uSockBuffsize);
        a_connectInfo.m_uUdpport = ntohl(a_connectInfo.m_uUdpport);
        a_connectInfo.m_uPacksize = ntohl(a_connectInfo.m_uPacksize);
}
                                                                                                   
/***************************************************************************************
   
  	This method is used for converting PackInfo from host byte order
        to network byte order
                          
 ***************************************************************************************/          
void Sabul::htonl_packInfo(LostPacks &a_packInfo)
{
	if ((a_packInfo.num != SYN) && (a_packInfo.num != END) )
	{
		for ( int i =0; i< (int)a_packInfo.num; i++ ) 
		{
			a_packInfo.lostSeq[i] = htonl(a_packInfo.lostSeq[i]);
		}
	}
	if ( a_packInfo.num == SYN ) 
	{
		a_packInfo.lostSeq[0] = htonl(a_packInfo.lostSeq[0]);
	}
	a_packInfo.num = htonl(a_packInfo.num);
	a_packInfo.ack = htonl(a_packInfo.ack);
		
}
/***************************************************************************************           
 
        This method is used for converting PackInfo from network byte order
        to host byte order

 ***************************************************************************************/                                                                                                   
void Sabul::ntohl_packInfo(LostPacks &a_packInfo)
{
	a_packInfo.num = ntohl(a_packInfo.num);
	a_packInfo.ack = ntohl(a_packInfo.ack);
	if ((a_packInfo.num != SYN) && (a_packInfo.num != END) )
        { 
	        for ( int i =0; i< (int)a_packInfo.num; i++ )
        	{
                	a_packInfo.lostSeq[i] = ntohl(a_packInfo.lostSeq[i]);
        	}
	}
	if ( a_packInfo.num == SYN )
	{
		a_packInfo.lostSeq[0] = ntohl(a_packInfo.lostSeq[0]);
	}
}
   
/***************************************************************************************
                                                                                                 
        This method is used for converting PackSeqNo from network byte order
        to host byte order                                                                         

 ***************************************************************************************/                                                                                                      
void Sabul::ntohl_packSeqNo(unsigned long &a_lpackSeqNo)
{
	a_lpackSeqNo = ntohl(a_lpackSeqNo);
}

/***************************************************************************************           

        This method is used for converting PackSeqNo from network byte order
        to host byte order
 
 ***************************************************************************************/          
void Sabul::htonl_packSeqNo(unsigned long &a_lpackSeqNo)
{
	a_lpackSeqNo = htonl(a_lpackSeqNo);
}                                           

/*****************************************************************************************
		
	This method is used for setting up the connection information 

*****************************************************************************************/
void Sabul::setupConnectInfo(int a_uRate=50, unsigned long a_uBuffsize=2000000,
	 int a_uUdpport=2000 )
{
    	m_ConnectInfo.m_uType=0; 				// for ConnectControl struct
    	m_ConnectInfo.m_uRate= a_uRate; 			// assume trans m_uRate is 100 mb/s
    	m_ConnectInfo.m_uSockBuffsize= a_uBuffsize; 		// unit byte
    	m_ConnectInfo.m_uUdpport= a_uUdpport;			// default 2000
    	m_ConnectInfo.m_uPacksize= MAX_PKT;  			// packet size = 1400;
}

/*****************************************************************************************
		
	This method is used for deciding the agreeable rate between the Server and 
	Client. The minimum of the two advertised rates is being set as the initial
	data transfer rate 

*****************************************************************************************/
void Sabul::decideConnectInfo(ConnectControl & a_pClientInfo)
{
    m_ConnectInfo.m_uRate=  min(a_pClientInfo.m_uRate, m_ConnectInfo.m_uRate);
    m_ConnectInfo.m_uSockBuffsize=  min(a_pClientInfo.m_uSockBuffsize, m_ConnectInfo.m_uSockBuffsize);
}


/*****************************************************************************************
		
	This method is used for Opening up a Sabul Socket for data transmission.
	This creates the TCP and UDP sockets and set up relevant connection setup
	information . The same method is used for Server and Client side Opens 
	by changing the the value of a_iFlag accordingly. 	

*****************************************************************************************/
int Sabul::Open(char * a_pcAddr, int a_iRate, unsigned long a_iBuf, int a_iFlag=0) // 0 is for server
{
	ConnectControl ClientInfo;
	char * c_pcAddr = new char[strlen(a_pcAddr)+1];
	
	strcpy(c_pcAddr, a_pcAddr);
	
       	m_tcpsocket.TcpOpen(c_pcAddr, a_iFlag);
       
       	setupConnectInfo(a_iRate, a_iBuf, 2000); 
       	// args value according to later modification. ( from sys calls, etc)

       	if(a_iFlag==0)  // Server Open Calls
       	{
          	m_tcpsocket.TcpRead(&ClientInfo, sizeof(struct ConnectControl)); 
		ntohl_connectInfo(ClientInfo);
          	if(ClientInfo.m_uType!=0)
		{ 
			cerr<< " info m_uType wrong" << endl;
		}
          	else
          	{
             		decideConnectInfo( ClientInfo);
             		::modStringAddr(c_pcAddr, m_ConnectInfo.m_uUdpport);
	     		int iTempRet = m_udpsocket.UdpOpen(c_pcAddr, a_iFlag);
	     		if(iTempRet <0)
			{
               		   	cerr<<"udp bind port error"<<endl;
			}
			else
			{
				m_ConnectInfo.m_uUdpport=iTempRet;
			}
			htonl_connectInfo(m_ConnectInfo);
             		m_tcpsocket.TcpWrite(&m_ConnectInfo, sizeof(struct ConnectControl));
			ntohl_connectInfo(m_ConnectInfo);
          	}
       	}
       	else if(a_iFlag==1) //Client Open Calls
       	{
		htonl_connectInfo(m_ConnectInfo);
         	m_tcpsocket.TcpWrite(&m_ConnectInfo,sizeof(struct ConnectControl));
         	m_tcpsocket.TcpRead(&m_ConnectInfo,sizeof(struct ConnectControl));
		ntohl_connectInfo(m_ConnectInfo);
		if(m_ConnectInfo.m_uUdpport==-1)
                {
                        cerr<<"couldn't get valid m_uUdpport num "<< endl;
                        return -1;
                }     
         	::modStringAddr(c_pcAddr, m_ConnectInfo.m_uUdpport);
         	m_udpsocket.UdpOpen(c_pcAddr, a_iFlag);


       	}
	delete [] c_pcAddr;
   	return 1;
}

/*****************************************************************************************
		
	This method is used calculating the time taken for transmitting an UDP packet
	with the given data rate  in microseconds

*****************************************************************************************/


int Sabul::getInterval(int rate, int packSize)
{
	if ( rate==0 ) 
	{
		rate = 1;
	}
    	return  packSize*8/rate;
}

/*****************************************************************************************
		
	This method is used calculating the total number of packets for the given
	buffer to be transmitted / received. 

*****************************************************************************************/

int Sabul::getPackNum(float buffersize, int packSize)
{
    // packSize's unit is Byte
    return  ((int)((buffersize)-1)/packSize+1); 
}

/*****************************************************************************************
		
	This method is used for receiving lost packet information from the Receiver.

*****************************************************************************************/

void Sabul::TcpThread()
{
    	int    iTcpReadCount =0;
    	unsigned iLastgotseq = 0;
    	int    iLostCount = 0;

	cout << " Entering Tcpthread" << endl;

    	while(true) // Infinite loop till it receives END signal from the Receiver
    	{
        	memset((void *)&m_PackInfo, 0, sizeof(LostPacks));
        	m_tcpsocket.TcpRead((void *)&m_PackInfo,sizeof(LostPacks));
		ntohl_packInfo(m_PackInfo);
        	iTcpReadCount++;
 
        	if(m_PackInfo.num>0) // Check if any message is contained in the packet 
        	{
 
			/************************************************************************* 
					    If END signal then the receiver has
					    received all packets correctly
			***************************************************************************/
                	if(m_PackInfo.num==END)
                	{
			//>>>>>>>>>
                	pthread_mutex_lock(&(::mylock1));
                        	m_iLostNum=END;
			//>>>>>>>>>
                	pthread_mutex_unlock(&(::mylock1));
				cout << "Leaving TcpThread" << endl;
                        	return;
                	}
			/************************************************************************* 
				If SYN signal, then this indicates the total number of
				lost packets left to be received. The TCP Thread will
				not accept any more lost packets greater than this
				number. This is useful to avoid a big overflow of 
				lost packets repeatedly due to delay. 	
			***************************************************************************/
                	if(m_PackInfo.num==SYN)
                	{
                        	m_bLaterRound = true;
                        	m_iResendMax = m_PackInfo.lostSeq[0];
                        	continue;
                	}
			/************************************************************************* 
				Check to see we go beyond the maximum lost list to be received. 
				If yes, then reset the lost packet count and start receiving
				the new lost packets list.	
			***************************************************************************/
                	if((m_iLostNum+m_PackInfo.num) > m_iResendMax)
                	{
			//>>>>>>>>>
                	pthread_mutex_lock(&(::mylock1));
                        	m_iLostNum = 0;
			//>>>>>>>>>
                	pthread_mutex_unlock(&(::mylock1));
                	}
			/************************************************************************* 
			   Lock and update the lost array safely with the received lost packets 
			*************************************************************************/ 
                	pthread_mutex_lock(&(::mylock2));
                	memcpy((char *)(m_LostListArray+m_iLostNum),
                                (char *)(m_PackInfo.lostSeq),
                                m_PackInfo.num*sizeof(int));
                	pthread_mutex_unlock(&(::mylock2));
			/**** Update the lost packets number safely with lock ******************/
                	pthread_mutex_lock(&(::mylock1));
                	m_iLostNum+=m_PackInfo.num;
                	iLostCount += m_PackInfo.num;
                	pthread_mutex_unlock(&(::mylock1));
 
        	} // m_PackInfo > 0 end                            
            	m_iLastAck=m_PackInfo.ack; // receiver sends the last got pack seq
                                       // in m_PackInfo.ack field.
                                       // for later use.
     } // end of infinite while loop
}                                                           

/*****************************************************************************************
		
	This method is used for Sending the given buffer to the receiver error free.

*****************************************************************************************/
int Sabul::Send( char* a_cPtr, unsigned long a_iLength)
{
 
 
    	int                 iInterval=getInterval(m_ConnectInfo.m_uRate,m_ConnectInfo.m_uPacksize);
    	unsigned long 	    iPackCount=getPackNum(a_iLength,m_ConnectInfo.m_uPacksize);
    	unsigned long       iPackLength=m_ConnectInfo.m_uPacksize;
    	long                lSleepTime;
    	//>>>>>>>>>> char    	    acLostPacket[1400];
    	char    	    acLostPacket[MAX_PKT];
    	unsigned long       	lSeqNo=0,  resentCount=0, packSum = iPackCount,
                        	iLostCount=0; // record the value of m_iLostNum;
    	unsigned long *piTempLostArray;
        int iLastPackLength=(int)(a_iLength-(iPackCount-1)*m_ConnectInfo.m_uPacksize);
        int iTotalPacketsSentInSecondRound = 0;
        int iTimeOut =0;
        bool bCheckContinue=false;
    	struct timeval      currtime, oldtime, starttime;  // system structure

	int 	iLostPackets = 0, iNoOfPktsTx=0, iState=0, iAckState =0;
	unsigned long  lSeqNoPassed = 0;
	unsigned long  lLastAck = 0;
	unsigned long  lTempSeqNo = 0;

	piTempLostArray= new unsigned long[ iPackCount];
        m_iPackCount = iPackCount;
        iLostCount = 0;
        m_iResendMax = m_iPackCount;
        m_LostListArray = new int [iPackCount];        
	m_iLastAck = 0;
	
	pthread_t  *tcpthreadp;

	pthread_mutex_init(&(::mylock1), NULL);
	pthread_mutex_init(&(::mylock2), NULL);

	/******** Copying last packet into a correct UDP buffer ******************/ 
        memcpy(acLostPacket,a_cPtr+m_ConnectInfo.m_uPacksize*(iPackCount-1),iLastPackLength);
 
    	if(a_iLength>m_ConnectInfo.m_uSockBuffsize)
    	{
          	cout<< " a_iLength, buffer length passed to Send is: "<< a_iLength << endl;
          	cout<< "not proper buffer size" << endl;
          	cout<< "m_ConnectInfo.m_uSockBuffsize is:::::::"<<m_ConnectInfo.m_uSockBuffsize<<endl;
         	return 1;
    	}
	/******** Create the Thread to receive lost packets ***************/	
    	if(m_TcpThreadNum==0)
    	{
       		tcpthreadp=(pthread_t*)malloc(sizeof(pthread_t));
       		pthread_create(tcpthreadp,NULL,::caller,(void *)this);
       		m_TcpThreadNum++;
    	}

        cout << "Initial Rate = " << m_ConnectInfo.m_uRate << endl;
        cout << "Interval is " << iInterval << endl;
        cout << "m_lSeqNo = " << m_lSeqNo << endl;
        for (int i = 0; i< iPackCount; i++)
        {
                piTempLostArray[i] = 0;
        }
        FILE *fp = fopen("debug.txt","w");                                                                            

	/************ Transmit all the packets  -- Called First Round Transmission ************/  
      	while(iPackCount>0)                                                                                          
      	{
        	if ( lSeqNo != 0)
        	{
			/******* Delay for transmitting at specified rate **********/
                	while(true)
                	{
                        	gettimeofday(&currtime,   NULL );
                        	lSleepTime =  iInterval- (1000000*(currtime.tv_sec-oldtime.tv_sec)+
                                	currtime.tv_usec-oldtime.tv_usec) ;
                        	if(lSleepTime < 0)
                        	{
                                	break;
                        	}
                	}
                	oldtime = currtime;
        	}
        	else /***    For getting timing information *********/ 
        	{
            		gettimeofday(&oldtime, NULL )  ;
            		gettimeofday(&starttime, NULL )  ;
        	}
 
        	if((int)iPackCount==1) // Sending of last packet since this contains smaller data than the UDP packet
        	{
			lTempSeqNo = lSeqNo + m_lSeqNo;
			//---------- htonl_packSeqNo(lTempSeqNo);
                	m_udpsocket.UdpWrite(acLostPacket, iPackLength,&lTempSeqNo);
			//---------- ntohl_packSeqNo(lTempSeqNo);
			//>>>>>>>>>>>
			fprintf(fp, "JJ%d\n", lTempSeqNo);
        	}
        	else // Sending of all other packets
        	{
			lTempSeqNo = lSeqNo + m_lSeqNo;
			//---------- lSeqNoPassed = htonl(lTempSeqNo);
			lSeqNoPassed = lTempSeqNo;
                	m_udpsocket.UdpWrite(a_cPtr+m_ConnectInfo.m_uPacksize*lSeqNo, iPackLength,&lSeqNoPassed);
			//>>>>>>>>>>>
			fprintf(fp, "KK%d\n", lTempSeqNo);
			
        	}
 
                //>>>>>>>>>> if(lSeqNo %500 == 0)
                if((m_iLastAck!=0)&&(lSeqNo %5000 == 0))
                {
//                        fprintf(fp,"NoofPkts=%d\tTotalLost=%d\tLostPktsinPrev=%d\tDataRateBefore=%d\tlastAck =%d",
//                                        lSeqNo,m_iLostNum, m_iLostNum-iLostPackets,m_ConnectInfo.m_uRate,
//                                        m_iLastAck);
 
                        //Check for Packet Loss and update the iInterval
                        pthread_mutex_lock(&(::mylock1));   
			{
			    int iOnePercent = (int)(m_iLastAck/100.0);
                            if((iAckState<5))
                            {
                                if(m_iLostNum > (m_iLastAck/2))
                                {
                                        m_ConnectInfo.m_uRate = m_ConnectInfo.m_uRate /2;
                                        iInterval=getInterval(m_ConnectInfo.m_uRate,m_ConnectInfo.m_uPacksize);
                                }
                                else if((m_iLostNum) <= iOnePercent)
                                {
                                        if((m_ConnectInfo.m_uRate + 1) < 700)
                                        {
                                                m_ConnectInfo.m_uRate += 1;
                                        }
                                        iInterval=getInterval(m_ConnectInfo.m_uRate,m_ConnectInfo.m_uPacksize);
                                }
                                else
                                {
                                        if((m_ConnectInfo.m_uRate - 1)> 1)
                                        {
                                                m_ConnectInfo.m_uRate -= 1;
                                        }
                                        iInterval=getInterval(m_ConnectInfo.m_uRate,m_ConnectInfo.m_uPacksize);
                                }
                                if((m_iLostNum - iLostPackets) < (m_iLastAck - lLastAck)/100)
                                        iState++;
                                else
                                        iState=0;
                                if(iState > 3)
                                        m_ConnectInfo.m_uRate += 3;
                                if (m_ConnectInfo.m_uRate > 1000)
                                        m_ConnectInfo.m_uRate = 1000;
                            }
                                iLostPackets = m_iLostNum;
                                if(lLastAck != m_iLastAck)
                                {
                                        lLastAck= m_iLastAck;
                                        iAckState = 0;
                                }
                                else
                                {
                                        iAckState++;
                                }
                        }
                        pthread_mutex_unlock(&(::mylock1));
 //                       fprintf(fp,"\tDataRateAfter=%d\n",m_ConnectInfo.m_uRate);
                        if(lSeqNo%5000 ==0)
                                cout << "DataRate is " << m_ConnectInfo.m_uRate << endl;
//>>>>>>>>>>>>
		iInterval=getInterval(1000 ,m_ConnectInfo.m_uPacksize);
                }                                                                        
			                                    
        	iPackCount--;
        	lSeqNo++;
      	}
 
        gettimeofday(&currtime, NULL )  ;
        cout << "Time taken for Initial Round = " << (1000000*(currtime.tv_sec-starttime.tv_sec)+
                        currtime.tv_usec-starttime.tv_usec) << endl;

	cout << "Current Rate = "<< m_ConnectInfo.m_uRate << endl;
        iInterval=getInterval(m_ConnectInfo.m_uRate,m_ConnectInfo.m_uPacksize);
                                                                                    
	/********************************************************************************** 
		    Subsequent Rounds - Read lost packets from Receiver using
		    data from TcpThread and retransmit those packets till receiving END signal 
	***********************************************************************************/ 
 
    	while(1)                                                                                                      
    	{
 
        	pthread_mutex_lock(&(::mylock1));
        	{
			/*********** If END signal received then all packets received successfully. ********/
           		if(m_iLostNum == END)
           		{
                		cout << "Total Packets Resent = " <<    iTotalPacketsSentInSecondRound++ << endl;
				m_iLostNum =0;
				//>>>>>>>>>>
        			pthread_mutex_unlock(&(::mylock1));
                		break;
           		}
			/******If some lost packets then copy the new list of lost packets *********/
           		else if(m_iLostNum != 0) 
           		{
           			pthread_mutex_lock(&(::mylock2));
                		memcpy((char*)  (piTempLostArray),
                                		(char *)m_LostListArray,
                                		sizeof(int)*m_iLostNum);
           			pthread_mutex_unlock(&(::mylock2));
                		iLostCount=m_iLostNum;
           		}
			/************************************************************************ 
					If no lost list received, wait for a while before you	
					start retransmitted the first packet of the previous
					lost list again 
			************************************************************************/ 
           		else
           		{
                		iTimeOut++;
                		if(iTimeOut < 100)
                		{
                        		bCheckContinue = true;
                		}
                     		iLostCount = 1;
           		}
           		m_iLostNum = 0;
        	}
        	pthread_mutex_unlock(&(::mylock1));
		/********************************************************************************** 
				Used for timeout delay so that you dont retransmit the first packet
			     	for the previous lost list very often 
		***********************************************************************************/	 
        	if(bCheckContinue)
        	{
                	bCheckContinue = false;
                	continue;
        	}
        	iTimeOut = 0;

		/********** Retransmit the  lost packets in piTempLostArray **************************/  
 
        	for(unsigned long iLoopVar=0;iLoopVar < iLostCount ; iLoopVar++)
        	{
		//>>>>>>>>>>> 
           		if(m_iLostNum == END)
           		{
                	//	cout << "Total Packets Resent = " <<    iTotalPacketsSentInSecondRound++ << endl;
				//m_iLostNum =0;
			//>>>>>>>>>
				iLostCount = 0;
				cout<<"LLLLLLLLLLLLLL"<<endl;
                		break;
				
			}
		//>>>>>>>>>> 

                	if( piTempLostArray[iLoopVar] > packSum )
			{
                        	break;
			}
                	while(true)
                	{
                       		gettimeofday(&currtime,   NULL );
                       		lSleepTime =  1*iInterval- (1000000*(currtime.tv_sec-oldtime.tv_sec)+
                                    currtime.tv_usec-oldtime.tv_usec) ;
                       		if(lSleepTime < 0)
                       		{
                        	        break;
                       		}
                	}
                	oldtime = currtime;
	                if(lSeqNo %100)
                        {
                                /*************************************************************************
                                          Check for Packet Loss and update the iInterval
                                 *************************************************************************/
                        }
         
			/***************** Sending last packet ******************/
                	if(piTempLostArray[iLoopVar] == (packSum-1))
                	{
				lTempSeqNo = piTempLostArray[iLoopVar] + m_lSeqNo;
				//---------- htonl_packSeqNo(lTempSeqNo);
                        	m_udpsocket.UdpWrite(acLostPacket, iPackLength,
	                                (long unsigned int *)(&(lTempSeqNo)));
			 	//---------- ntohl_packSeqNo(lTempSeqNo);
			//>>>>>>>>>>>
			fprintf(fp, "LL%d\n", lTempSeqNo);
                	}
                	else /****** Sending all other packets regularly ************/ 
                	{
				lTempSeqNo = piTempLostArray[iLoopVar] + m_lSeqNo;
				//---------- lSeqNoPassed = htonl(lTempSeqNo);
				lSeqNoPassed = lTempSeqNo;
                        	m_udpsocket.UdpWrite(a_cPtr+m_ConnectInfo.m_uPacksize*(piTempLostArray[iLoopVar]),
                               		iPackLength, (long unsigned int *)(&lSeqNoPassed));
			//>>>>>>>>>>>
			fprintf(fp, "MM%d\n", lTempSeqNo);
				
                	}
                	iTotalPacketsSentInSecondRound++;
                	resentCount++;
        	}
				//cout<<"ppp"<<iLostCount<<"uuu"<<m_iLostNum<<"  ";
  	} // end of Infinite While loop
  	cout << "resend Count = " << resentCount<< endl;
	free(tcpthreadp);
	fclose(fp);
	delete [] piTempLostArray;
	delete [] m_LostListArray;
	cout << "Before = m_lSeqNo = " << m_lSeqNo << endl;
	m_lSeqNo += m_iPackCount-1;
        m_TcpThreadNum = 0;
        pthread_join(*tcpthreadp,NULL);
        cout << "After = m_lSeqNo = " << m_lSeqNo << endl;  
        return 0;
}
                               

/*****************************************************************************************
		
	This method is used for Sending the list of Lost packets to the Sender
	during the reception of first round. It sends the list of lost packets
	starting from a_iWaitSeqNo till a_iWaitSeqNo + a_iLostPackNum	

*****************************************************************************************/
void  Sabul::sendFirstRoundLostList(unsigned long  a_iWaitSeqNo, int a_iLostPackNum) 
{
    	int  iMaxCount;
	unsigned long  iLostPackSeqNo;

    	m_PackInfo.num =0;
    	iMaxCount =0;
    	memset((void *)&m_PackInfo,  0, sizeof(LostPacks));
	/***************************************************************
		Embed all the lost packets in the m_PackInfo structure
		and then send the packet. A maximum of 350 lost list
		numbers can be embedded in this structure which 
		has been designed to keep it under 1400 bytes (MTU size
		of Ethernet) If more than 350 consecutive lost packets
		they are sent as more than one packet 
	*****************************************************************/ 
    	for (int i = 0; i< a_iLostPackNum; i++ ) 
	{
        	iLostPackSeqNo = a_iWaitSeqNo + i;
        	m_PackInfo.lostSeq[iMaxCount] = iLostPackSeqNo ;
        	m_iLostNum++;
		m_PackInfo.num ++;
        	iMaxCount ++;
        	//>>>>>>>>>> if (iMaxCount == 350 )  
        	if (iMaxCount == (MAX_PKT/4))  
		{
			iLostPackSeqNo = a_iWaitSeqNo + i;
			htonl_packInfo(m_PackInfo);			
            		m_tcpsocket.TcpWrite((void *) &m_PackInfo, sizeof(m_PackInfo));
            		iMaxCount=0;
	    		memset((void *)&m_PackInfo,  0, sizeof(LostPacks));
        	}
    	}
	m_PackInfo.ack = a_iWaitSeqNo + a_iLostPackNum;
	htonl_packInfo(m_PackInfo);
     	m_tcpsocket.TcpWrite((void *) &m_PackInfo, sizeof(m_PackInfo));
}

/*****************************************************************************************

		This method is used for sending lost packets after the first round.
		It takes the array containing the seq numbers of packets not yet received,
		begining seq number and ending seq number and the size of the array. 
		It then searched through the array till it reaches to the begining
		seq number and sends all packets upto the end sequence number specified
		to the sender. Similar to the first round, it sends a maximum of 350 
		lost packet sequence numbers on one packet.	
  
*****************************************************************************************/
void  Sabul::sendSecondRoundLostList(unsigned long *a_LostListArray, unsigned long a_lBegin,
						unsigned long a_lEnd, unsigned long a_lTotal) 
{

    	m_PackInfo.num=0;
    	memset((void *)&m_PackInfo,  0, sizeof(LostPacks));
	m_PackInfo.num = 0;
	for (int i=0;i<(int)a_lTotal;i++)
	{
	
		/*** Send all seq numbers > a_lBegin and <= a_lEnd  to the Sender*****/
		if(a_LostListArray[i] >a_lBegin)
		{ 
			m_PackInfo.lostSeq[m_PackInfo.num++] = a_LostListArray[i];
			/****** We can send only 350 seq numbers in one packet and hence this check ****/ 
			//>>>>>>>>>> if(m_PackInfo.num >= 350)
			if(m_PackInfo.num >= (MAX_PKT/4))
			{
				m_PackInfo.ack = a_LostListArray[i];
				htonl_packInfo(m_PackInfo);
    				m_tcpsocket.TcpWrite((void *) &m_PackInfo, sizeof(m_PackInfo));
    				memset((void *)&m_PackInfo,  0, sizeof(LostPacks));
				m_PackInfo.num = 0;
			}
		}
		if(a_LostListArray[i] >a_lEnd)
		{ 	
			m_PackInfo.ack = a_lEnd; 
			htonl_packInfo(m_PackInfo);
                        m_tcpsocket.TcpWrite((void *) &m_PackInfo, sizeof(m_PackInfo));
    			memset((void *)&m_PackInfo,  0, sizeof(LostPacks));
			m_PackInfo.num = 0;
			return;
		}
		
	}
	m_PackInfo.ack = a_lEnd;
	htonl_packInfo(m_PackInfo);
    	m_tcpsocket.TcpWrite((void *) &m_PackInfo, sizeof(m_PackInfo));
}

/*****************************************************************************************
		This is the Recv method which receives data from the sender onto the
		a_pBuf buffer. The length of data to be received is given to the Recv
		method 
*****************************************************************************************/

int  Sabul::Recv(char* a_pBuf, unsigned long a_iLength) 
{
 
  	unsigned long  lWaitSeqNo = 0;
  	unsigned long  lSeqNo = 0;
  	unsigned long  lTotalPkgNum;
  	unsigned long  lSumPkg;
  	unsigned long  lLostPackNum;
  	unsigned long  lPkgCount =0; 	// used for pkg count
	unsigned long  l50Count =0;     // used for pkg count
 
	unsigned long  lLastPktRcvd =0;
        unsigned long  lNoOfTimesLastPktRcvd = 0; 
  	//>>>>>>>>>> char  acLastPacket[1400]; 	
  	char  acLastPacket[MAX_PKT]; 	
  	int            iPkgLen = m_ConnectInfo.m_uPacksize;
  	bool  bFirstRound = true; 
	unsigned long *pLostListArray, lLostListMax = 0;
	bool bContinueFlag = false; 
  	struct timeval	oldtime, newtime, secondroundtimebegin, secondroundtimeend;  
  	int iLastPkgLen, iTotalSecondRound = 0, iInterval;
        unsigned int iRetVal = 0; 

   	a_iLength= (a_iLength<m_ConnectInfo.m_uSockBuffsize) ? a_iLength : m_ConnectInfo.m_uSockBuffsize;
  	lTotalPkgNum = getPackNum(a_iLength, iPkgLen) ;                                                                                         
  	iLastPkgLen = (int) (a_iLength-((lTotalPkgNum-1)*m_ConnectInfo.m_uPacksize));
  	lSumPkg= lTotalPkgNum;
	pLostListArray = new unsigned long [lSumPkg]; 

	//>>>>>>>>>>
        FILE *fp = fopen("debug.txt","w");                                                                            

	/******************************************************************************************
		Infinite while loop till the entire buffer is received correctly  
	******************************************************************************************/
   	while (true) 
   	{
 
		/**************************************************************************** 
		   Check if we are expecting the last packet, If so, receive it in a 
		   special buffer
		****************************************************************************/
     		if(lWaitSeqNo==(lTotalPkgNum-1))
		{
         		m_udpsocket.UdpRead( acLastPacket,  iPkgLen, (unsigned long *)&lSeqNo ) ;
			//---------- ntohl_packSeqNo(lSeqNo);
		//>>>>>>>>>>>>>>>>>>
			//fprintf(fp,"JJ%d   \n", lSeqNo);

			if((lSeqNo < m_lSeqNo)|| (lSeqNo > (m_lSeqNo+lTotalPkgNum-1)))
                        {
                                continue;
                        }    
			lSeqNo = lSeqNo - m_lSeqNo; 
			if(lWaitSeqNo == lSeqNo)
			{	
                       		memcpy( a_pBuf+lSeqNo*iPkgLen, acLastPacket , iLastPkgLen) ;
			}	
		}
		/************* Read the packet in expected Buffer location directly ************************/ 
     		else 
		{
			iRetVal = m_udpsocket.UdpRead( (a_pBuf + (lWaitSeqNo * iPkgLen)),  iPkgLen, 
				(unsigned long *)&lSeqNo ) ;       
			//---------- ntohl_packSeqNo(lSeqNo);
		//>>>>>>>>>>>>>>>>>>
			//fprintf(fp,"KK%d   \n", lSeqNo);

			lSeqNo = lSeqNo - m_lSeqNo;
                        if((lSeqNo > (m_lSeqNo+lTotalPkgNum-1)))
                        {
                                continue;
                        }  
		}

		/************** Used for calculating time ***********************************/
     		if ( lPkgCount == 0 )  
		{
			gettimeofday(&oldtime, NULL );
     		}
		/****************  First Round Receiving Packets ****************************/
           	if ( bFirstRound) 
		{                                                                                                    
				/*************************************************************  		
					  Condition 1 - received packet sequence number
					  greater than the expected sequence number 
				**************************************************************/	
                		if ( lSeqNo > lWaitSeqNo )
				{
					l50Count = 0; 
					/********* Copy the packet in right location *********/
		     			if ( lSeqNo ==(lTotalPkgNum-1))
					{ 
                         			memcpy( a_pBuf+lSeqNo*iPkgLen, a_pBuf+ 
							lWaitSeqNo * iPkgLen, iLastPkgLen) ;
					}	
		     			else 
					{
						memcpy( a_pBuf+lSeqNo*iPkgLen, a_pBuf+ 
							lWaitSeqNo * iPkgLen, iPkgLen) ; 
					}
					/******** Update the lost list packets appropriately  *********/
                     			lLostPackNum= lSeqNo - lWaitSeqNo;
					for(unsigned int i = lWaitSeqNo; i< lSeqNo; i++)
					{
					       pLostListArray[lLostListMax++] = i;
					}
					/********** Send the lost list to Sender immediately *********/ 
		     			sendFirstRoundLostList(lWaitSeqNo, lLostPackNum);
                     			lWaitSeqNo = lSeqNo+1;
		     			lPkgCount++;
                		}       
				/******************************************************************
					Condition 2 = Packet Received correctly  
				******************************************************************/
				else if ( lWaitSeqNo == lSeqNo ) 
				{
					lWaitSeqNo ++;
                     			lPkgCount++;
					l50Count++;
                                        if(l50Count ==  50)
                                        {
                                                m_PackInfo.num = 0 ;
                                                m_PackInfo.ack = lWaitSeqNo-1 ;
                                                m_tcpsocket.TcpWrite((void *) &m_PackInfo, sizeof(m_PackInfo));
                				l50Count = 0;  
                                        }
                //>>>>>>>>    should be in upper lines.                     l50Count = 0;  
	 			}

				/******************************************************************
					Condition 3 =  Seq No of packet reeceived is less
					than the expected sequence number  
				******************************************************************/

				else
				{
					l50Count=0;  
					/******* Copy the packet in right buffer location *********/
					if(lWaitSeqNo == lTotalPkgNum-1)
					{	
                       				memcpy( a_pBuf+lSeqNo*iPkgLen, acLastPacket , iPkgLen) ;
					}	
					else
					{	
		    				memcpy( a_pBuf+lSeqNo*iPkgLen, a_pBuf+ lWaitSeqNo * iPkgLen, 
						iPkgLen) ;
					}

					/************************************************************
						In this condition the packet received could be
						out of order or the Sender is retransmitting the
						lost packets - Hence appropriate action has to be taken
					**************************************************************/
 
		    			lLostPackNum = lSumPkg - lWaitSeqNo;
					for(unsigned long i=0; i< lLostListMax; i++) 
					{
						/******** Remove the packet received from lost list *****/
						if(pLostListArray[i] == lSeqNo)
						{
							memmove(&(pLostListArray[i]),
								&(pLostListArray[i+1]),
								(lLostListMax-(i+1))*sizeof(unsigned long));	
                     					lPkgCount++;
							lLostListMax = lLostListMax - 1;
							if((i+lSumPkg-lWaitSeqNo)< 50)	
							{
								bContinueFlag = true;
							}
							break;
						}
						if(pLostListArray[i] > lSeqNo)
						{
							break;
						}
					}	
					/************************************************************** 
						    If out of order packet then go and try
						    to receive the previous expected packet
						    again.		
					***********************************************************/	
					if( ((lWaitSeqNo - lSeqNo) < 50) || (bContinueFlag == true))
					{
						bContinueFlag = false;
						continue;
					}	
					/************************************************************** 
						    If not out of order packet then send the 
						    remaining lost packets list and add
						    them to the lost list 
					***********************************************************/	
					if((lWaitSeqNo - lSeqNo) > 50)
					{
		    				sendFirstRoundLostList(lWaitSeqNo, lLostPackNum);
						for(unsigned int i = lWaitSeqNo; i< lSumPkg; i++)
						{
					       		pLostListArray[lLostListMax++] = i;
						}
					}
 				} // End of check for waitSeqNo > iseqNo 

				/********************************************************************
					The following code is executed only when the 
					Receiver receives the second round of packets
					which is the lost packets from the Sender so that
					Receiver can follow a slightly different algo
					for receiving lost packets
				*******************************************************************/ 

		     		if (( lSeqNo == (lSumPkg-1) ) || ((lWaitSeqNo-1) > lSeqNo)) 
				{
					/***** Print first round stats ****************/
					gettimeofday(&newtime, NULL);
					iInterval = 1000000*(newtime.tv_sec-oldtime.tv_sec)+
                        			newtime.tv_usec-oldtime.tv_usec; 			
					cout << "iInterval= " << iInterval<<
						"  pkgCount= "<< lPkgCount<< 
						"  sumPkg= " << lSumPkg<<
						" lostPkg=" << lSumPkg - lPkgCount  << 
						"Array Lost Max  = " << lLostListMax << endl;
					/**********************************/
					memset((void *)&m_PackInfo, 0, sizeof(LostPacks));
       	     				m_PackInfo.num = SYN;
					/***** Send the Sender the total number of lost packets ****/
					m_PackInfo.lostSeq[0] = lSumPkg - lPkgCount;
					m_PackInfo.ack = lSumPkg-1 ; 
					htonl_packInfo(m_PackInfo);
                			m_tcpsocket.TcpWrite((void *) &m_PackInfo, sizeof(m_PackInfo));
                    			bFirstRound = false;
					/***** Update the next wait seq No correctly **********/ 
					if(lWaitSeqNo == lTotalPkgNum)
					{
						lWaitSeqNo = pLostListArray[0];
					}
					else		
					{
						for(unsigned long i=0; i< lLostListMax; i++) 
						{
							if(pLostListArray[i] > lSeqNo)
							{
								lWaitSeqNo = pLostListArray[i];
								break;	
							}
						}	
					}
		     		}
				gettimeofday(&secondroundtimebegin, NULL);

            	}// End of First Round check

		/****************  Subsequent Round Receiving Packets ****************************/

            	else if ( !bFirstRound ) 
		{
				/****************************************************************
					Condition 1 - Received packet is same as expected
				****************************************************************/ 
				if ( lWaitSeqNo == lSeqNo )  
				{
					for(unsigned long i=0; i< lLostListMax; i++) 
					{
						/************************************************** 
							Remove the packet from lost list
							if present and update values accordingly
						***************************************************/
						if(pLostListArray[i] == lSeqNo)
						{
							memmove(&(pLostListArray[i]),
								&(pLostListArray[i+1]),
								(lLostListMax-(i+1))*sizeof(unsigned long));	
							lLostListMax--;
							lPkgCount ++;
							if(i != lLostListMax)
							{
								lWaitSeqNo = pLostListArray[i];
							}
							else
							{
								lWaitSeqNo = pLostListArray[0];
							}
						}
						if(pLostListArray[i] > lSeqNo)
						{
							break;
						}
					}	
                		}
                		else  
				{
					/****************************************************************
						Condition 2 - 	Received packet seq No is
								less than expected 
					****************************************************************/ 
		     			if ( lSeqNo < lWaitSeqNo ) 
					{
						unsigned long lPrevWaitSeqNo = lWaitSeqNo; 	
						/******************************************************* 
							  Check whether the same packet is being
							  received multiple times. Used for
							  detecting a particular lock condition 
							  and come out of it.
						*******************************************************/
							   
						if(lLastPktRcvd != lSeqNo)
						{
							lLastPktRcvd = lSeqNo;
							lNoOfTimesLastPktRcvd = 0;
						}
						else
						{
							lNoOfTimesLastPktRcvd++;
						}		
						/*****************************************************
							If the packet is on the lost list, remove
							it and update the next waiting seq No 
						********************************************************/
						for(unsigned long i=0; i< lLostListMax; i++) 
						{
							if(pLostListArray[i] == lSeqNo)
							{
								if(lWaitSeqNo == lTotalPkgNum-1)
								{	
                       							memcpy( a_pBuf+lSeqNo*iPkgLen, acLastPacket , iPkgLen) ;
								}	
		     						else 
								{
                        						memcpy( (a_pBuf+(lSeqNo*iPkgLen)), 
									(a_pBuf+ (lWaitSeqNo * iPkgLen)), iPkgLen) ;
								}
								memmove(&(pLostListArray[i]),
									&(pLostListArray[i+1]),
									(lLostListMax-(i+1))*sizeof(unsigned long));	
								lLostListMax--;
								lPkgCount ++;
								if((i) != lLostListMax)
								{
									lWaitSeqNo = pLostListArray[i];
								}
								else
								{
									lWaitSeqNo = pLostListArray[0];
								}
				//>>>>>>>>>>>>>>>>>>>>>
						break;
							}
							if(pLostListArray[i] > lSeqNo)
							{
								break;
							}
						}	
						/******** Check if the same packet is more than received twice **********/
						if(lNoOfTimesLastPktRcvd < 3)
						{
							continue;
						}
						/**********************************************************************  
							If same packet is being received then send a SYN
							signal and resend the entire lost list to Sender
						**********************************************************************/  
						lNoOfTimesLastPktRcvd = 0;
						memset((void *)&m_PackInfo, 0, sizeof(LostPacks));
						m_PackInfo.num = SYN;
						m_PackInfo.lostSeq[0] = lLostListMax;
						htonl_packInfo(m_PackInfo);
       					 	m_tcpsocket.TcpWrite((void *) &m_PackInfo, sizeof(m_PackInfo));
						for(unsigned long i=0; i< lLostListMax; i++) 
						{
							if(pLostListArray[i] > lSeqNo)
							{
								lWaitSeqNo = pLostListArray[i];
								break;
							}
						}

                     				sendSecondRoundLostList(pLostListArray,lPrevWaitSeqNo,
							99999999, lLostListMax); 
                     				sendSecondRoundLostList(pLostListArray,0,lSeqNo,
							lLostListMax); 
						iTotalSecondRound++;
                     			}  
					/****************************************************************
						Condition 3 - 	Received packet seq No is
								greater than expected 
					****************************************************************/ 
  		     			else 
					{
						unsigned long lPrevWaitSeqNo = lWaitSeqNo; 	
						/******************************************************* 
							  Check whether the same packet is beingnn
							  received multiple times. Used for
							  detecting a particular lock condition 
							  and come out of it.
						*******************************************************/
						if(lLastPktRcvd != lSeqNo)
						{
							lLastPktRcvd = lSeqNo;
							lNoOfTimesLastPktRcvd = 0;
						}
						else
						{
							lNoOfTimesLastPktRcvd++;
						}		
						/*****************************************************
							If the packet is on the lost list, remove
							it and update the next waiting seq No 
						********************************************************/
						for(unsigned long i=0; i< lLostListMax; i++) 
						{
							if(pLostListArray[i] == lSeqNo)
							{
		     						if (lSeqNo  ==(lTotalPkgNum-1)) 
								{
									memcpy( a_pBuf+lSeqNo*iPkgLen, 
										a_pBuf+ lWaitSeqNo * iPkgLen, iLastPkgLen) ;  
								}
		     						else 
								{
                        						memcpy( (a_pBuf+(lSeqNo*iPkgLen)), 
										(a_pBuf+ (lWaitSeqNo * iPkgLen)), iPkgLen) ;
								}
								memmove(&(pLostListArray[i]),
									&(pLostListArray[i+1]),
									(lLostListMax-(i+1))*sizeof(unsigned long));	
								lLostListMax--;
								lPkgCount ++;
								if((i) != lLostListMax)
								{
									lWaitSeqNo = pLostListArray[i];
								}
								else
								{
									lWaitSeqNo = pLostListArray[0];
								}
				//>>>>>>>>>>>>>>>>>>>>>
						break;
							}
							if(pLostListArray[i] > lSeqNo)
							{
								break;
							}
						}	
						/***************  Send the lost packets inbetween to Sender *********/
                     				sendSecondRoundLostList(pLostListArray,lPrevWaitSeqNo,lSeqNo,
								lLostListMax); 
						/******** Check if the same packet is more than received twice **********/
						if(lNoOfTimesLastPktRcvd < 3)
						{
							continue;
						}
						/**********************************************************************
							If the same packet is received more than twice
							resend the lost list to Sender for retransmission.
						***********************************************************************/	
						lNoOfTimesLastPktRcvd = 0;
                     				sendSecondRoundLostList(pLostListArray,0,lSeqNo,
							lLostListMax); 
		     			}

                		} // end of condition (seqNo != expected SeqNo) 
            	} // end of subsequent rounds
          	// The check ipkgCount  == itotalPkgNum is for the first round 
		if(!lLostListMax && !bFirstRound)
		{
			/************ OOOOOOOOHHHHHHH HHAAAAA !!! Received all Packet :-) Return SUCCESS ***********/
           		break;
		}     
   	}
	//>>>>>>>>>>
	fclose(fp);
	gettimeofday(&secondroundtimeend, NULL);
 	cout << "Total Second Rounds = " <<	iTotalSecondRound++ << endl;
	cout << "Time Taken in second round = " << 1000000*(secondroundtimeend.tv_sec-secondroundtimebegin.tv_sec)+
                                                secondroundtimeend.tv_usec-secondroundtimebegin.tv_usec;  
  	gettimeofday(&newtime, NULL);
  	memset((void *)&m_PackInfo,  0, sizeof(LostPacks));
  	m_PackInfo.num = END;
	htonl_packInfo(m_PackInfo);
  	m_tcpsocket.TcpWrite((void *) &m_PackInfo, sizeof(m_PackInfo));
	m_lSeqNo += lTotalPkgNum-1;
  	return 0;
}

void Sabul::Close()
{
	if(m_tcpsocket.TcpClose()==-1)
	{
		cout<<"TcpClose() error"<<endl;
		exit(1);
	}
	if(m_udpsocket.UdpClose()==-1)
	{
		cout<<"UdpClose() error"<<endl;
		exit(1);
	}
}


#include "sabul.h"
#include "ipaddr.h"
 
#define Mess "hello, server"
int main(int argc, char* argv[])
{
    Sabul sabulsock;
    int status;
    int iIteration =ROUND;	

    struct timeval  oldtime, newtime;
    gettimeofday(&oldtime, NULL);
    int * buf=new int[32000000];
    for ( int i = 0; i< 32000000; i++) 
	buf[i] = i;

    gettimeofday(&newtime, NULL);
    int interval= 1000000*(newtime.tv_sec-oldtime.tv_sec)+newtime.tv_usec-oldtime.tv_usec;
 cout << "time used for initiating data is "<< interval<< "usec"<<endl;
    
    status=sabulsock.Open(argv[1],atoi(argv[2]), 128000000,1);
	for(int i=0; i< iIteration; i++)
	{
    sabulsock.Send((char*)buf, 128000000);
	}

    /* 
    if(tcpsock.TcpWrite(Mess,strlen(Mess))<0) return 1;

    if(tcpsock.TcpRead(buf, sizeof(buf))<0) return 1;
    cerr<<"client:receive msg:"<<buf<<endl;
 
    //tcpsock.TcpWrite(Mess,strlen(Mess));
    
    //delete buf;
    //
    */
    sabulsock.Close();
    return 0;
}              

#include "sabul.h"
#include "ipaddr.h"

#define Mess  "hello, client"
int main(int argc, char* argv[])
{
    Sabul  sabulsock;
    int status;
    int *buf=new int[32000000];
    int iIteration =ROUND;	

    struct timeval  oldtime, newtime;

    status=sabulsock.Open(argv[1],atoi(argv[2]), 128000000,0);  

    gettimeofday(&oldtime, NULL); 

	for (int i = 0; i < iIteration; i++)
	{
    sabulsock.Recv((char*)buf, 128000000);
	}

    sabulsock.Close();

    gettimeofday(&newtime, NULL);
    int interval= 1000000*(newtime.tv_sec-oldtime.tv_sec)+newtime.tv_usec-oldtime.tv_usec;
cout << "interval = "<< interval << " (usec) "  << endl;


   cout << "Data Rate = " << iIteration* 128*8.0 / (interval)*1000*1000 << "Mbps"<< endl;

	bool bError=false;	


	FILE *fp = fopen("error.log","w"); 
    //>>>>>>>>>>> for(int i=0;i<32000000;i+= 350)
    for(int i=0;i<32000000;i+= (MAX_PKT/4))
    {	
	bError = false;	
	//>>>>>>>>> for (int j=i; j < i+350; j++) 
	for (int j=i; j < i+(MAX_PKT/4); j++) 
	{
		if(j >= 8000000)
			break;	
      		if(buf[j]!=j) 
      		//if((buf[j]!=j) && (buf[j] != 0)) 
        	{ 	
			//>>>>>>>>>> fprintf(fp, "Tobe %d - Contains Val%d -Pkt# %d - Containsvalue od Pkt#%d, The Pkt address offset is %d \n",  j,buf[j],j/350,buf[j]/350, (&(buf[j])-&(buf[0])) ); 
			fprintf(fp, "Tobe %d - Contains Val%d -Pkt# %d - Containsvalue od Pkt#%d, The Pkt address offset is %d \n",  j,buf[j],j/(MAX_PKT/4),buf[j]/(MAX_PKT/4), (&(buf[j])-&(buf[0])) ); 
        		//cout<< j << "-" << buf[j] << "-" << j/350 << "-" << buf[j]/350 <<"  ";
			bError=true;
			break;
		}
	}
	if(bError)
	{
	  fprintf(fp,"----------------------------------------------------------------------------------\n");
	  printf("----------------------------------------------------------------------------------\n");
	}
    }
	
    cout << endl; 
   return 0;
}

     

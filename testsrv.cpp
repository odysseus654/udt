
#include <pthread.h>
#include "sbl.h"
#define COUNT 5000



int main(int argc, char* argv[])
{

	timeval *tv00 = new timeval;
        timeval *tv11 = new timeval;
	double counts= COUNT;
	double timepast;
	double start_time;
	double end_time;
	Sbl_Srv* sbl = new Sbl_Srv();
	int port=DEFAULT_PORT;

	//USAGE:: ./testsrv PreferPort RequiredRate

	if(argc==3)
	{
	   sbl->setRate(atol(argv[2]));
	}

	if(argc==2)
	{
	  port = atoi(argv[1]);
	}
	
	
        sbl->Connect_Open(port);


        gettimeofday(tv00, 0);
	start_time=(double)clock();

        char * userdata;  
	long size=BLK_SIZE*PAYLOAD_SIZE;
	int i,j;

	while(counts>0)
        {
		counts--;

           	userdata = new char[size];
           	for (j = 0; j <size;j=j+1000)
              		userdata[j] = (char)j;
	   	sbl->Send_Data((void*)userdata, size);

                //cout << "round finished\n";
                //sleep(5);
        }

        sbl->Connect_Close();

        gettimeofday(tv11, 0);
	end_time=(double)clock();

        timepast=(tv11->tv_sec - tv00->tv_sec) + 1e-6* (tv11->tv_usec - tv00->tv_usec);
	counts=COUNT*(double)size;
	printf("size & overall size = %d   %e\n",  size ,  counts);
	cout<<"Time and Rate are: "<<timepast<<"       "<< 1e-6*size*8*COUNT/timepast<<endl;
	cout<<"Time used by program"<<(end_time-start_time)/CLOCKS_PER_SEC<<endl;
	cout<<"Utilization= "<<(end_time-start_time)/CLOCKS_PER_SEC/timepast<<endl;


	delete tv00;
	delete tv11;
	return 0;

}

#include <pthread.h>
#include "sbl.h"

#define SBL_CLT




int main(int argc, char* argv[])
{
	int port=DEFAULT_PORT;
	Sbl_Clt* sbl = new Sbl_Clt();

	if(argc==1)
	{
		cout<<"error parameters"<<endl;
		cout<<"usage: "<<argv[0] <<" hostname hostport RequiredRate"<<endl;
		exit(-1);
	}

	if(argc>=3)
	{
	   port=atoi(argv[2]);
	}

	if(argc==4)
	{
	   sbl->setRate(atol(argv[3]));
	}

	//sbl = new Sbl_Clt(rate);
	sbl->Connect_Open(argv[1],port);

        char * userdata;
        int size=BLK_SIZE*PAYLOAD_SIZE;
	int k;

	//Provide buffer a little more at the beginning
	//so that SLEEP in RECV_DATA do not have bad effect.

	userdata=new char[size];
	sbl->Prov_Buff(userdata, size);
	userdata=new char[size];
	sbl->Prov_Buff(userdata, size);
	userdata=new char[size];
	sbl->Prov_Buff(userdata, size);
	userdata=new char[size];
	sbl->Prov_Buff(userdata, size);
	userdata=new char[size];
	sbl->Prov_Buff(userdata, size);
	userdata=new char[size];
	sbl->Prov_Buff(userdata, size);

	while(true)
	{
		userdata=new char[size];
		sbl->Prov_Buff(userdata, size);
		userdata=(char*)sbl->Recv_Data(size);
		delete [] userdata;
	}

	sbl->Connect_Close();
	return 0;
}


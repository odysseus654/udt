#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <recver.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char* argv[])
{
   if ((argc < 3) or (0 == atoi(argv[2])))
   {
      cout << "usage: appclient server_address server_port" << endl;
      return 0;
   }

   if (argc > 3)
   {
      cout << "usage: appclient server_address server_port" << endl;
      cout << "parameters omited" << endl;
   }

   CSabulRecver* recver = new CSabulRecver;
   try
   {
      recver->open(argv[1], atoi(argv[2]));
   }
   catch(...)
   {
      cout << "Failed to Open New Sabul Connection. Program Aborted." << endl;
      return 0;
   }

   int size = 7340000;
   char* data = new char[size];

   for (int i = 0; i < 1000; i++)
      recver->recv(data, size);

   cout << data << endl;

   delete [] data;

   recver->close();

   return 1;
}

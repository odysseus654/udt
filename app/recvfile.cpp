#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <recver.h>

int main(int argc, char* argv[])
{
   if ((argc < 4) || (0 == atoi(argv[2])))
   {
      cout << "usage: recvfile server_address server_port filename" << endl;
      return 0;
   }

   if (argc > 4)
   {
      cout << "usage: appclient server_address server_port filename" << endl;
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

   unlink(argv[3]);
   int fd = open(argv[3], O_RDWR | O_CREAT);

   recver->recvfile(fd, 0, 528);
   close(fd);

   recver->close();

   return 1;
}

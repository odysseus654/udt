#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <sender.h>

int main(int argc, char* argv[])
{
   if ((argc > 2) || ((2 == argc) && (0 == atoi(argv[1]))))
   {
      cout << "usage: appserver [server_port]" << endl;
      cout << "parameters omited" << endl;
   }

   CSabulSender* sender = new CSabulSender;

   bool block = false;
   sender->setOpt(SBL_BLOCK, &block, sizeof(block));

   try
   {
      if (argc > 1)
         sender->open(atoi(argv[1]));
      else
         sender->open();
   }
   catch(...)
   {
      cout << "Failed to Open New Sabul Server. Program Aborted." << endl;
      return 0;
   }

   try
   {
      sender->listen();
   }
   catch(...)
   {
      cout << "Failed to Open New Sabul Server. Program Aborted." << endl;
      return 0;
   }

   timeval time1, time2;

   char* data;
   int size = 7340000;

   gettimeofday(&time1, 0);

   for (int i = 0; i < 1000; i ++)
   {
      data = new char[size];
      try
      {
         while (!(sender->send(data, size)))
            usleep(10);
         if (block)
            delete [] data;
      }
      catch(...)
      {
         cout << "connection broken ...\n";
         return 0;
      }
   }

   while (sender->getCurrBufSize())
      usleep(10);

   gettimeofday(&time2, 0);
   cout << "speed = " << 60000.0 / double(time2.tv_sec - time1.tv_sec + (time2.tv_usec - time1.tv_usec) / 1000000.0) << "Mbits/sec" << endl;

   sender->close();

   return 1;
}

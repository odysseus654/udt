#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <sender.h>

int main(int argc, char* argv[])
{

   if (argc < 2)
   {
      cout << "usage: sendfile filename [port_number]" << endl;
      return 0;
   }

   if (argc > 2)
   {
      cout << "usage: sendfile filename [port_number]" << endl;
      cout << "parameters omited" << endl;
   }

   CSabulSender* sender = new CSabulSender;

   try
   {
      if (argc > 2)
         sender->open(atoi(argv[2]));
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

   timeval t1, t2;

   gettimeofday(&t1, 0);

   int fd = open(argv[1], O_RDONLY);
   struct stat fs;
   stat(argv[1], &fs);

   sender->sendfile(fd, 0, fs.st_size);

   while (sender->getCurrBufSize())
      usleep(10);

   sender->close();

   gettimeofday(&t2, 0);

   cout << "speed = " << (double(fs.st_size) * 8. / 1000000.) / (t2.tv_sec - t1.tv_sec + (t2.tv_usec - t1.tv_usec) / 1000000.) << endl;

   return 1;
}

#include <fstream>

int main()
{
   ofstream data;
   data.open("sequence.dat", ios::out || ios::binary);

   long size = sizeof(int);

   for (int i = 0; i < 20000000; i ++)
      data.write((char *)&i, size);

   data.close();

   return 0;
}

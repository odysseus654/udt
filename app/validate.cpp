#include <fstream>

int main()
{
   fstream data;

   data.open("sequence.dat", ios::in);

   int val;
   int size = sizeof(int);
   int orig = -1;


   for (int i = 0; i < 20000000; i ++)
   {
      data.read(&val, size);

      if (val != orig + 1)
         cout << val << "  " << orig << endl;

      orig = val;

   }

}

#include <fstream>
#include <stdlib.h>
#include <stdio.h>

//r 0.01012 0 1 UDT 1500 ------- 3 0.0 1.0 -1 0


inline double getsecoftime(const char* trace)
{
   char temp[32];

   int i = 2;
   while (trace[i] != ' ')
   {
      temp[i - 2] = trace[i];
      i ++;
   }
   temp[i - 2] = '\0';

   return atof(temp);
}

inline int getid(const char* trace)
{
   int i = 0;

   while (trace[i] != '-') i ++;

   while (trace[i] != ' ') i ++;

   i ++;
   
   char temp[8];
   int c = 0;

   while (trace[i] != ' ') temp[c ++] = trace[i ++];
   temp[c] = '\0';

   return atoi(temp);
}

inline int getdest(const char* trace)
{
   char temp[8];
   int i, c = 0;

   for (i = 0; i < 3; i ++)
   {
      while (trace[c] != ' ') c ++;
      c ++;
   }

   i = 0;

   while (trace[c] != ' ') temp[i ++] = trace[c ++];

   temp[i] = '\0';

   return atoi(temp);
}

inline int getsize(const char* trace)
{
   char temp[8];
   int i, c = 0;

   for (i = 0; i < 5; i ++)
   {
      while (trace[c] != ' ') c ++;
      c ++;
   }

   i = 0;

   while (trace[c] != ' ') temp[i ++] = trace[c ++];

   temp[i] = '\0';

   return atoi(temp);
}

double parse(ifstream* trace, const int& id, const int& dest)
{
   char output[32];
   sprintf(output, "flow%d.trace", id);

   ofstream result(output);
   char temp[256];
   int time = 0;
   int unitcount;

   double sum = 0.0;

   while(!trace->eof())
   {
      trace->getline(temp, 256);
      unitcount = 0;
      while (!trace->eof() && (time == int(getsecoftime(temp))))
      {
         if (('r' == temp[0]) && (id == getid(temp)) && (dest == getdest(temp)) && (1500 == getsize(temp)))
            unitcount ++;

         trace->getline(temp, 256);
      }
      result << unitcount * 3 / 250.0 << ", ";
      sum += unitcount * 3 / 250.0;
      cout << time << " finished!\n";
      time ++;
   }
   
   cout << endl;

   return sum;
}


int main(int argv, char** argc)
{
   ifstream trace;

   trace.open("out.tr");
   cout << parse(&trace, atoi(argc[1]), atoi(argc[2])) << endl;

/*   
   for (int i = 0; i < 20; i++)
   {
      trace.open("out.tr");
      parse(&trace, i);
      trace.close();
   }
*/

   return 0;
}

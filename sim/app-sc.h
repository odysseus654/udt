#ifndef _APP_SC_H_
#define _APP_SC_H_

#include "webcache/tcpapp.h"

class AppScData: public AppData
{
public:
   AppScData(AppDataType t_, long attr = 0, long* loss = NULL);
   AppScData(AppScData& asc);

   ~AppScData();

   virtual void pack(long attr,  long* loss = NULL);
   virtual int size() const { return sizeof(AppScData); }
   virtual AppScData* copy() { return new AppScData(*this); };

   inline long& attr() { return attr_; }
   inline long* loss() { return loss_; }

private:
   long attr_;
   long* loss_;
};

class AppSc: public TcpApp
{
enum status {server, client};

public:
   AppSc(Agent* a);
   int command(int argc, const char*const* argv);
   virtual void process_data(int size, AppData* data);

private:
   status s_;
};

#endif

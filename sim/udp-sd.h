#ifndef _UDP_SD_H_
#define _UDP_SD_H_

#include "object.h"
#include "udp.h"
#include "timer-handler.h"
#include "app.h"


struct hdr_sd
{
  int size_;
  int& size() { return size_; }

  static int offset_;
  static int& offset() { return offset_; }
  static hdr_sd* access(const Packet* p)
  {
    return (hdr_sd*) p->access(offset_);
  }
};

class UdpSdAgent: public Agent
{
enum status {server, client};
public:
  UdpSdAgent();
  int command(int argc, const char*const* argv);
  virtual void send(int size, AppData* data);
  virtual void recv(Packet* p, Handler*);

private:
  Application*  app1_; //?????????
  int udpsd_hdr_size_;
  status s_;
};

#endif

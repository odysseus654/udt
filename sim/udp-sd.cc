#include "udp-sd.h"
#include "app-sd.h"
#include "app-sc.h"
#include "ip.h"

int hdr_sd::offset_;

static class SabulDataHeaderClass: public PacketHeaderClass
{
public:
  SabulDataHeaderClass(): PacketHeaderClass("PacketHeader/SabulData", sizeof(hdr_sd))
  {
    bind_offset(&hdr_sd::offset_);
  }
} class_sd_hdr;

static class UdpSdAgentClass: public TclClass
{
public:
  UdpSdAgentClass(): TclClass("Agent/UdpSd") {}
  TclObject* create(int, const char*const*)
  {
    return new UdpSdAgent;
  }
} class_udp_sd;

UdpSdAgent::UdpSdAgent(): Agent(PT_SD), s_(client)
{
  bind("udpsd_hdr_size_", &udpsd_hdr_size_);
}

int UdpSdAgent::command(int argc, const char*const* argv)
{
  if (3 == argc)
  {
    if (strcmp(argv[1], "set-status") == 0)
    {
      if (strcmp(argv[2], "server") == 0)
        s_ = server;
      else if (strcmp(argv[2], "client") == 0)
        s_ = client;

      return TCL_OK;
    }
  }

  return Agent::command(argc, argv);
}

void UdpSdAgent::send(int size, AppData* data)
{
  Packet* p;

  if (server == s_)
  {
    p = allocpkt(data->size());
    hdr_sd* sh = hdr_sd::access(p);
    sh->size() = data->size();

    p->setdata(data);

    hdr_cmn* ch = hdr_cmn::access(p);
    ch->size() = /*udpsd_hdr_size_ +*/ size;
    Agent::send(p, 0);
  }
  else
  {
    p = allocpkt(data->size());
    hdr_sd* sh = hdr_sd::access(p);
    sh->size() = data->size();

    p->setdata(data);

    hdr_cmn* ch = hdr_cmn::access(p);
    ch->size() = /*udpsd_hdr_size_ +*/ size;
    Agent::send(p, 0);
  }
}

void UdpSdAgent::recv(Packet* p, Handler*)
{
  if (server == s_)
  {
    hdr_ip* ih = hdr_ip::access(p);
    if ((ih->saddr() == addr()) && (ih->sport() == port()))
      // XXX Why do we need this?
      return;
    hdr_sd* sh = hdr_sd::access(p);
    ((AppSd *) app_)->process_data(sh->size(), p->userdata());

    Packet::free(p);
  }
  else
  {
    hdr_ip* ih = hdr_ip::access(p);
    if ((ih->saddr() == addr()) && (ih->sport() == port()))
      // XXX Why do we need this?
      return;
    hdr_sd* sh = hdr_sd::access(p);
    ((AppSd *) app_)->process_data(sh->size(), p->userdata());

    Packet::free(p);
  }
}

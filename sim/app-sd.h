#ifndef _APP_SD_H_
#define _APP_SD_H_

#include "udp-sd.h"
#include "packet.h"
#include "timer-handler.h"
#include "app.h"


class AppSdData: public AppData
{
public:
   AppSdData(): AppData(SD_PKT) {};
   AppSdData(AppDataType t_, int s_ = 0): AppData(t_), seqno_(s_) {};
   AppSdData(AppSdData& asd): AppData(asd) { seqno_ = asd.seqno(); }

   virtual const int size() {return sizeof(AppSdData); }
   virtual void pack(int s_ = 0) {seqno_ = s_; };
   virtual AppSdData* copy() {return new AppSdData(*this);};

   inline int& seqno() {return seqno_;};

   inline void init() {};

private:
   int seqno_;
};

class AppSd;

class SendTimer: public TimerHandler
{
public:
   SendTimer(AppSd* t): TimerHandler(), t_(t) {}
   inline virtual void expire(Event*);

private:
   AppSd* t_;
};

class ScExpTimer: public TimerHandler
{
public:
   ScExpTimer(AppSd* t): TimerHandler(), t_(t) {}
   inline virtual void expire(Event*);
private:
   AppSd* t_;
};

class ScErrTimer: public TimerHandler
{
public:
   ScErrTimer(AppSd* t): TimerHandler(), t_(t) {}
   inline virtual void expire(Event*);
private:
   AppSd* t_;
};

class ScSynTimer: public TimerHandler
{
public:
   ScSynTimer(AppSd* t): TimerHandler(), t_(t) {}
   inline virtual void expire(Event*);
private:
   AppSd* t_;
};

class ScAckTimer: public TimerHandler
{
public:
   ScAckTimer(AppSd* t): TimerHandler(), t_(t) {}
   inline virtual void expire(Event*);
private:
   AppSd* t_;
};

class LossList
{
public:
   LossList();
   LossList(const long& seqno);

   void insert(const long& seqno);
   void insertattail(const long& seqno);
   void remove(const long& seqno);
   void removeall(const long& seqno);
   long getlosslength() const;
   long getfirstlostseq() const;
   void getlossarray(long* array, int& len, const int& limit, const double& interval) const;

private:
   long attr_;
   double timestamp_;
   LossList* next_;
   LossList* tail_;
};

class AppSd: public Application
{
friend class AppSc;

enum status {server, client};

public:
   AppSd();
   virtual ~AppSd();
   int command(int argc, const char*const* argv);
   virtual void process_data(int size, AppData* data);
   virtual void send(int nbytes);

   inline int& pktsize() { return pktsize_; };

   void feedback(AppDataType t);

//private:
public:
   status s_;

   ScExpTimer sc_exp_timer_;
   ScAckTimer sc_ack_timer_;
   ScErrTimer sc_err_timer_;
   ScSynTimer sc_syn_timer_;

   SendTimer snd_timer_;

   double expinterval_;
   double ackinterval_;
   double errinterval_;
   double syninterval_;

   bool freeze_;
   double increase_;
   int deccount_;
   bool lastinc_;
   int lastdec_;

   double history[64];
   bool init;
   int hp;

   double rtt_;
   double interval_;
   int pktsize_;
   int maxlosslen_;

   int sendflagsize_;
   int recvflagsize_;

   int errcount_;
   int losscount_;

   long lastack_;
   long localsend_;
   long localrecv_;
   long localerr_;
   long currseqno_;
   long nextexpect_;

   double lossratelimit_;
   double threshold_;
   double weight_;
   double historyrate_;

   LossList* losslist_;

   int* recvflag_;

private:
   void calcnextexpt(const int& currseq);
};

#endif

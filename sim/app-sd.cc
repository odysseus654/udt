#include "app-sd.h"
#include "app-sc.h"
#include "udp-sd.h"
#include "math.h"

static class AppSdClass: public TclClass
{
public:
   AppSdClass(): TclClass("Application/AppSd") {};
   TclObject* create(int, const char*const*)
   {
      return new AppSd;
   };
} class_app_sd;

void SendTimer::expire(Event*)
{
   t_->send(t_->pktsize());
}

void ScAckTimer::expire(Event*)
{
   t_->feedback(SC_ACK);
}

void ScExpTimer::expire(Event*)
{
   AppScData asc(SC_EXP);   
   t_->process_data(asc.size(), &asc);
}

void ScErrTimer::expire(Event*)
{
   t_->feedback(SC_ERR);
}

void ScSynTimer::expire(Event*)
{
   t_->feedback(SC_SYN);
}

//
LossList::LossList():
attr_(0),
next_(NULL)
{
   tail_ = this;
}

LossList::LossList(const long& seqno):
attr_(seqno),
next_(NULL)
{
   timestamp_ = 0.; //Scheduler::instance().clock();
}

void LossList::insert(const long& seqno)
{
   LossList *p = new LossList(seqno);

   LossList *q = this;

   while ((NULL != q->next_) && (q->next_->attr_ < seqno))
      q = q->next_;

   if ((NULL != q->next_) && (seqno == q->next_->attr_))
      return;

   p->next_ = q->next_;
   q->next_ = p;

   attr_ ++;
}

void LossList::insertattail(const long& seqno)
{
   LossList *p = new LossList(seqno);

   tail_->next_ = p;
   tail_ = p;

   attr_ ++;
}

void LossList::remove(const long& seqno)
{
   LossList *p = this;

   while ((NULL != p->next_) && (p->next_->attr_ < seqno))
      p = p->next_;

   if ((NULL != p->next_) && (p->next_->attr_ == seqno))
   {
      LossList *q = p->next_;
      p->next_ = q->next_;
      if (tail_ == q)
         tail_ = p;

      delete q;

      attr_ --;
   }

}

void LossList::removeall(const long& seqno)
{
   LossList *p = this;
   LossList *q;

   while ((NULL != p) && (NULL != p->next_) && (p->next_->attr_ <= seqno))
   {
      q = p->next_;
      p->next_ = q->next_;
      if (tail_ == q)
         tail_ = p;
      delete q;

      attr_ --;

      p = p->next_;
   }
}

long LossList::getlosslength() const
{
   return attr_;
}

long LossList::getfirstlostseq() const
{
   return (NULL == next_) ? attr_ : next_->attr_;
}

void LossList::getlossarray(long* array, int& len, const int& limit, const double& interval) const
{
   LossList *p = this->next_;

   double currtime = Scheduler::instance().clock();

   for (len = 0; (NULL != p) && (len < limit); p = p->next_)
   {
     if (currtime - p->timestamp_ > interval)
      {
         array[len] = p->attr_;
         p->timestamp_ = Scheduler::instance().clock();
         len ++;
      }
   }
}


AppSd::AppSd(): Application(), s_(client), snd_timer_(this), sc_ack_timer_(this), sc_exp_timer_(this), sc_syn_timer_(this), sc_err_timer_(this)
{
   bind("pktsize_", &pktsize_);
   bind("interval_", &interval_);
   bind("maxlosslen_", &maxlosslen_);
   bind("threshold_", &threshold_);
   bind("rtt_", &rtt_);
   bind("ackinterval_", &ackinterval_);
   bind("expinterval_", &expinterval_);
   bind("syninterval_", &syninterval_);
   bind("errinterval_", &errinterval_);
   bind("sendflagsize_", &sendflagsize_);
   bind("recvflagsize_", &recvflagsize_);

   lastack_ = 0;
   currseqno_ = 0;
   nextexpect_ = 0;
   localsend_ = 0;
   localrecv_ = 0;
   localerr_ = 0;

   lossratelimit_ = 0.001;
   historyrate_ = 0;
   weight_ = 0.0;

   freeze_ = false;
   lastdec_ = -1;
   increase_ = 0;
   init = true;
   hp = 0;

   losslist_ = new LossList;
   recvflag_ = new int [recvflagsize_ * 2];
   for (int i = 0; i < recvflagsize_ * 2; i ++)
      recvflag_[i] = 0;
}

AppSd::~AppSd()
{
   delete losslist_;
   delete [] recvflag_;
}

int AppSd::command(int argc, const char*const* argv)
{
   Tcl& tcl = Tcl::instance();

   if (3 == argc)
   {
      if (0 == strcmp(argv[1], "set-status"))
      {

         if (0 == strcmp(argv[2], "server"))
            s_ = server;
         else if (0 == strcmp(argv[2], "client"))
            s_ = client;

         return TCL_OK;
      }
      else if (0 == strcmp(argv[1], "set-target"))
      {
         target_ = (AppSc*)TclObject::lookup(argv[2]);
         return TCL_OK;
      }
   }
   else if (2 == argc)
   {
      if (0 == strcmp(argv[1], "initialize"))
      {
         return TCL_OK;
      }
      else if (0 == strcmp(argv[1], "start"))
      {
         if (server == s_)
         {
            send(pktsize_);
            sc_exp_timer_.resched(expinterval_);
         }
         else
         {
	    sc_ack_timer_.resched(ackinterval_);
            sc_syn_timer_.resched(syninterval_);
            sc_err_timer_.resched(errinterval_);
         }

         return TCL_OK;
      }
      else if (0 == strcmp(argv[1], "close"))
      {
 	 sc_exp_timer_.resched(1000000);
         sc_ack_timer_.resched(1000000);
         sc_syn_timer_.resched(1000000);
         sc_err_timer_.resched(1000000);
         snd_timer_.resched(1000000);

         return TCL_OK;
      }
   }

   return Application::command(argc, argv);
}

void AppSd::process_data(int size, AppData* data)
{
   double max, min, a, b, p, orig;
   int n;

   if (s_ == server)
   {
      int attr = ((AppScData*)data)->attr();


      switch (((AppScData*)data)->type())
      {
      case SC_ACK:
         if (attr <= lastack_)
            break;

         lastack_ = attr;
         
         losslist_->removeall(attr);

         break;

      case SC_ERR:
         localerr_ += attr;
         losscount_ += attr;

         if (((AppScData*)data)->loss()[0] > lastdec_)
         {
            printf("lossing %d int %f %d\n", errcount_, interval_, lastdec_);
            interval_ = interval_ * 1.125;

            freeze_ = true;

            lastdec_ = currseqno_;

            errcount_ = 1;
            deccount_ = 4;
         }
         else if (++ errcount_ >= pow(2, deccount_))
         {
            printf("lossing %d int %f %d\n", errcount_, interval_, ((AppScData*)data)->loss()[0]);
            
            interval_ = interval_ * 1.125;

            deccount_ ++;
         }

         for (int i = 0; i < attr; i ++)
            losslist_->insert(((AppScData*)data)->loss()[i]);

         break;

      case SC_SYN:
         //rate

         if (localerr_ >= 2)
         {
            localerr_ = 0;
	    break;
         }
         else 
         {
	    increase_ = pow(10, ceil(log10(syninterval_ / interval_))) / 1000.0;

            if (increase_ < 1.0/1500.0)
               increase_ = 1.0/1500.0;

            printf("increase %f\n", increase_);

            interval_ = (interval_ * syninterval_) / (interval_ * increase_ + syninterval_);
         }

         if (interval_ > threshold_)
            interval_ = threshold_;

         localsend_ = 0;
         localerr_ = 0;

         printf("sending rate %f %f\n", interval_, syninterval_);

         sc_syn_timer_.resched(syninterval_);

         break;

      case SC_EXP:
         for (int i = lastack_; i < currseqno_; i ++)
            losslist_->insertattail(i);

         break;

      default:
         break;
      }

      sc_exp_timer_.resched(expinterval_);

   }
   else
   {
      localrecv_ ++;

      int seqno = ((AppSdData *) data)->seqno();

      if (seqno < lastack_)
         return;

      if (seqno - lastack_ >= recvflagsize_)
      {
         feedback(SC_ACK);
         return;
      }

      if (1 == recvflag_[seqno - lastack_])
         return;

      if (seqno > currseqno_ + 1)
      {
         for (int i = currseqno_ + 1; i < seqno; i ++)
            losslist_->insertattail(i);

         feedback(SC_ERR);

         printf("loss %d %d\n",seqno, currseqno_);

         currseqno_ = seqno;
      }
      else if (seqno < currseqno_)
         losslist_->remove(seqno);
      else
         currseqno_ = seqno;

      recvflag_[seqno - lastack_] = 1;
   }
}

void AppSd::send(int nbytes)
{
   if (losslist_->getlosslength() > 0)
   {
      AppSdData* asd = new AppSdData;
      asd->pack(losslist_->getfirstlostseq());
      losslist_->remove(losslist_->getfirstlostseq());
      ((UdpSdAgent *) agent_)->send(pktsize_, asd);

      localsend_ ++;
   }
   else if (currseqno_ - lastack_ < sendflagsize_)
   {
      AppSdData* asd = new AppSdData;
      asd->pack(currseqno_);
      ((UdpSdAgent *) agent_)->send(pktsize_, asd);

      currseqno_ ++;
      localsend_ ++;
   }

   if (freeze_)
   {
      snd_timer_.resched(syninterval_ + interval_);

      printf("freeze \n");
      freeze_ = false;
   }
   else
      snd_timer_.resched(interval_);
}

void AppSd::feedback(AppDataType t)
{
   AppScData* asc = new AppScData(t);

   long* lossmsg = NULL;
   int losslen = 0;

   switch (t)
   {
   case SC_ACK:
      sc_ack_timer_.resched(ackinterval_);

      if (0 == losslist_->getlosslength())
         asc->pack(currseqno_ + 1);
      else
         asc->pack(losslist_->getfirstlostseq());

      if (asc->attr() <= lastack_)
         break;

      for (int i = 0; i < recvflagsize_; i ++)
         recvflag_[i] = recvflag_[(asc->attr() - lastack_) + i];
      lastack_ = asc->attr();

      ((UdpSdAgent *) agent_)->send(asc->size(), asc);

      break;

   case SC_ERR:
      sc_err_timer_.resched(errinterval_);

      lossmsg = new long [maxlosslen_];

      losslist_->getlossarray(lossmsg, losslen, maxlosslen_, rtt_ * 2); 

      if (losslen <= 0)
         break;

      asc->pack(losslen, lossmsg);

      delete [] lossmsg;

      ((AppSc *) target_)->send(asc->size(), asc);

      break;

   case SC_SYN:
      sc_syn_timer_.resched(syninterval_);

      asc->pack(localrecv_);

      ((AppSc *) target_)->send(asc->size(), asc);

      break;

   default:
      break;
   }
}

void AppSd::calcnextexpt(const int& currseq)
{
   nextexpect_ = (currseq + 1) % recvflagsize_;
   while (0 != recvflag_[nextexpect_])
      nextexpect_ = (nextexpect_ + 1) % recvflagsize_;
}

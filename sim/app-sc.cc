#include "app-sc.h"
#include "app-sd.h"
#include "string.h"

static class AppScClass: public TclClass
{
public:
   AppScClass(): TclClass("Application/AppSc") {};
   TclObject* create(int argc, const char*const* argv)
   {
      if (argc != 5)
         return NULL;
      Agent *a = (Agent *)TclObject::lookup(argv[4]);
      if (a == NULL)
         return NULL;
      return new AppSc(a);
   };
} class_app_sc;


AppScData::AppScData(AppDataType t_, long attr,  long* loss): AppData(t_)
{
   attr_ = attr;

   if (NULL == loss)
   {
      loss_ = NULL;
      return;
   }

   loss_ = new long[attr_];
   memcpy(loss_, loss, attr_ * sizeof(long));
}

AppScData::AppScData(AppScData& asc): AppData(asc.type())
{
   attr_ = asc.attr();

   if (SC_ERR != asc.type()) 
      return;

   loss_ = new long[attr_];
   memcpy(loss_, asc.loss_, attr_ * sizeof(long));
}

AppScData::~AppScData()
{
   if(loss_ != NULL)
   {
      delete [] loss_;
      loss_ = NULL;
   }
}

void AppScData::pack(long attr,  long* loss)
{
   attr_ = attr;

   if (SC_ERR != type()) 
      return;

   loss_ = new long[attr_];
   memcpy(loss_, loss, attr_ * sizeof(long));
}

AppSc::AppSc(Agent* a): TcpApp(a), s_(client)
{

}

int AppSc::command(int argc, const char*const* argv)
{
   Tcl& tcl = Tcl::instance();

   if (3 == argc)
   {
      if (strcmp(argv[1], "connect") == 0)
      {
         dst_ = (AppSc *)TclObject::lookup(argv[2]);
         if (dst_ == NULL)
         {
            tcl.resultf("%s: connected to null object.", name_);
            return (TCL_ERROR);
         }
         dst_->connect(this);
         tcl.resultf("ok!");
         return (TCL_OK);
      }
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
         target_ = (AppSd*)TclObject::lookup(argv[2]);
         return TCL_OK;
      }
   }

   return Application::command(argc, argv);
}

void AppSc::process_data(int size, AppData* data)
{
   if (server == s_)
   {
      (dynamic_cast<AppSd *>(target_))->process_data(size, data);
   }
   else
   {

   }
}

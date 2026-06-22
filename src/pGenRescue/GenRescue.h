/************************************************************/
/*    FILE: GenRescue.h                                    */
/************************************************************/

#ifndef GenRescue_HEADER
#define GenRescue_HEADER

#include <map>
#include <set>
#include <string>
#include <vector>
#include "MOOS/libMOOS/Thirdparty/AppCasting/AppCastingMOOSApp.h"

struct Swimmer
{
  std::string id;
  double x = 0;
  double y = 0;
  bool rescued = false;
};

class GenRescue : public AppCastingMOOSApp
{
 public:
   GenRescue();
   ~GenRescue() {}

 protected:
   bool OnNewMail(MOOSMSG_LIST &NewMail);
   bool Iterate();
   bool OnConnectToServer();
   bool OnStartUp();
   bool buildReport();

 protected:
   void registerVariables();
   bool handleSwimmerAlert(const std::string& alert);
   bool handleFoundSwimmer(const std::string& report);
   void postPathUpdate();
   void postRescueRequests();
   std::vector<std::string> buildGreedyOrder() const;
   double distTo(double x, double y, double px, double py) const;

 private:
   std::map<std::string, Swimmer> m_swimmers;
   std::set<std::string> m_found_ids;
   std::map<std::string, double> m_last_request_time;

   bool m_got_nav_x;
   bool m_got_nav_y;
   double m_nav_x;
   double m_nav_y;

   std::string m_update_var;
   std::string m_rescue_request_var;
   std::string m_vname;
   double m_request_range;
   double m_request_repeat;
   unsigned int m_updates_posted;
   unsigned int m_requests_posted;
};

#endif

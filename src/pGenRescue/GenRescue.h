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

struct Contact
{
  std::string name;
  double x = 0;
  double y = 0;
  double heading = 0;
  double timestamp = 0;
  bool valid = false;
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
   bool handleNodeReport(const std::string& report);
   void postPathUpdate();
   void postPeriodicPathUpdate();
   void postRescueRequests();
   std::vector<std::string> buildPathOrder() const;
   std::vector<std::string> getAvailableSwimmers() const;
   std::set<std::string> getConcededSwimmers() const;
   bool shouldConcede(const Swimmer& swimmer) const;
   bool contactIsFresh() const;
   double candidateScore(const std::string& id,
                         const std::vector<std::string>& remaining,
                         double cx, double cy) const;
   double closestNeighborDistance(const std::string& id,
                                  const std::vector<std::string>& remaining) const;
   double distTo(double x, double y, double px, double py) const;
   double relBearing(double from_x, double from_y, double heading,
                     double to_x, double to_y) const;

 private:
   std::map<std::string, Swimmer> m_swimmers;
   std::set<std::string> m_found_ids;
   std::map<std::string, double> m_last_request_time;
   Contact m_contact;

   bool m_got_nav_x;
   bool m_got_nav_y;
   double m_nav_x;
   double m_nav_y;

   std::string m_update_var;
   std::string m_rescue_request_var;
   std::string m_vname;
   std::string m_planner;
   bool m_use_adversary;
   double m_request_range;
   double m_request_repeat;
   double m_path_refresh_interval;
   double m_last_path_post;
   double m_adversary_concede_range;
   double m_adversary_heading_gate;
   double m_adversary_stale_time;
   double m_cluster_weight;
   unsigned int m_concede_count;
   unsigned int m_updates_posted;
   unsigned int m_requests_posted;
};

#endif

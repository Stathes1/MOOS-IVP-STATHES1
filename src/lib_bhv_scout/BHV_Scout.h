#ifndef BHV_SCOUT_HEADER
#define BHV_SCOUT_HEADER

#include <map>
#include <string>
#include <vector>
#include "IvPBehavior.h"
#include "XYPoint.h"
#include "XYPolygon.h"

struct ScoutContact {
  double x;
  double y;
  double time;
  std::string type;
};

class BHV_Scout : public IvPBehavior {
public:
  BHV_Scout(IvPDomain);
  ~BHV_Scout() {}
  bool setParam(std::string, std::string);
  void onIdleState();
  void onEveryState(std::string);
  IvPFunction* onRunState();

protected:
  IvPFunction* buildFunction();
  bool updateRegion();
  void buildCoverageRoute();
  double routeScore(const std::vector<XYPoint>&) const;
  void handleNodeReport(const std::string&);
  void postViewPoint(bool=true);
  void postViewRoute(bool=true);

protected:
  double m_osx;
  double m_osy;
  double m_curr_time;
  double m_ptx;
  double m_pty;
  bool m_pt_set;
  XYPolygon m_rescue_region;
  std::string m_region_spec;
  std::vector<XYPoint> m_route;
  unsigned int m_route_ix;
  unsigned int m_pass_count;
  std::map<std::string, ScoutContact> m_contacts;

  double m_capture_radius;
  double m_desired_speed;
  double m_lane_spacing;
  double m_contact_timeout;
  std::string m_tmate;
};

#define IVP_EXPORT_FUNCTION
extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string, IvPDomain domain)
  {return new BHV_Scout(domain);}
}
#endif

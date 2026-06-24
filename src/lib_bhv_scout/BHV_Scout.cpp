#include <algorithm>
#include <cmath>
#include <limits>
#include "BHV_Scout.h"
#include "AngleUtils.h"
#include "MBUtils.h"
#include "OF_Coupler.h"
#include "XYFormatUtilsPoly.h"
#include "XYSegList.h"
#include "ZAIC_PEAK.h"

using namespace std;

BHV_Scout::BHV_Scout(IvPDomain domain) : IvPBehavior(domain)
{
  IvPBehavior::setParam("name", "scout");
  m_osx = m_osy = m_curr_time = m_ptx = m_pty = 0;
  m_pt_set = false;
  m_route_ix = 0;
  m_pass_count = 0;
  m_desired_speed = 1.6;
  m_capture_radius = 5;
  m_lane_spacing = 28;
  m_contact_timeout = 20;
  m_history_spacing = 6;
  m_revisit_radius = 24;
  m_revisit_weight = 20;
  addInfoVars("NAV_X, NAV_Y, RESCUE_REGION");
  addInfoVars("SCOUTED_SWIMMER, NODE_REPORT");
}

bool BHV_Scout::setParam(string param, string val)
{
  param = tolower(param);
  if(param == "capture_radius")
    return setPosDoubleOnString(m_capture_radius, val);
  if(param == "desired_speed")
    return setPosDoubleOnString(m_desired_speed, val);
  if(param == "lane_spacing")
    return setPosDoubleOnString(m_lane_spacing, val);
  if(param == "contact_timeout")
    return setPosDoubleOnString(m_contact_timeout, val);
  if(param == "history_spacing")
    return setPosDoubleOnString(m_history_spacing, val);
  if(param == "revisit_radius")
    return setPosDoubleOnString(m_revisit_radius, val);
  if(param == "revisit_weight")
    return setPosDoubleOnString(m_revisit_weight, val);
  if(param == "tmate")
    return setNonWhiteVarOnString(m_tmate, val);
  return false;
}

void BHV_Scout::onEveryState(string)
{
  m_curr_time = getBufferCurrTime();
  if(getBufferVarUpdated("NODE_REPORT"))
    handleNodeReport(getBufferStringVal("NODE_REPORT"));

  if(!getBufferVarUpdated("SCOUTED_SWIMMER"))
    return;
  string report = getBufferStringVal("SCOUTED_SWIMMER");
  if(report == "")
    return;
  if(m_tmate == "") {
    postWMessage("Mandatory teammate name is null");
    return;
  }
  postOffboardMessage(m_tmate, "SWIMMER_ALERT", report);
  postEventMessage("Forwarded discovery to " + m_tmate + ": " + report);
}

void BHV_Scout::onIdleState()
{
  m_curr_time = getBufferCurrTime();
}

IvPFunction *BHV_Scout::onRunState()
{
  bool okx = false;
  bool oky = false;
  m_osx = getBufferDoubleVal("NAV_X", okx);
  m_osy = getBufferDoubleVal("NAV_Y", oky);
  if(!okx || !oky) {
    postWMessage("No ownship X/Y info in info_buffer");
    return 0;
  }
  if(!updateRegion())
    return 0;
  recordTrailPosition();
  if(m_route.empty())
    buildCoverageRoute();
  if(m_route.empty()) {
    postWMessage("Unable to construct coverage route");
    return 0;
  }

  if(!m_pt_set) {
    m_ptx = m_route[m_route_ix].get_vx();
    m_pty = m_route[m_route_ix].get_vy();
    m_pt_set = true;
  }

  if(hypot(m_ptx-m_osx, m_pty-m_osy) <= m_capture_radius) {
    m_route_ix++;
    if(m_route_ix >= m_route.size()) {
      m_pass_count++;
      postEventMessage("Coverage pass " + uintToString(m_pass_count) + " complete");
      m_route.clear();
      buildCoverageRoute();
      if(m_route.empty()) {
        m_pt_set = false;
        return 0;
      }
    }
    m_ptx = m_route[m_route_ix].get_vx();
    m_pty = m_route[m_route_ix].get_vy();
  }

  postViewPoint(true);
  postViewRoute(true);
  IvPFunction *ipf = buildFunction();
  if(!ipf)
    postWMessage("Problem creating scout IvP function");
  return ipf;
}

bool BHV_Scout::updateRegion()
{
  string spec = getBufferStringVal("RESCUE_REGION");
  if(spec == "") {
    postWMessage("Unknown RESCUE_REGION");
    return false;
  }
  postRetractWMessage("Unknown RESCUE_REGION");
  if(spec == m_region_spec)
    return true;

  XYPolygon region = string2Poly(spec);
  if(!region.is_convex() || region.size() < 3) {
    postWMessage("Badly formed RESCUE_REGION");
    return false;
  }
  m_rescue_region = region;
  m_region_spec = spec;
  m_route.clear();
  m_route_history.clear();
  m_route_ix = 0;
  m_pt_set = false;
  return true;
}

void BHV_Scout::buildCoverageRoute()
{
  vector<vector<XYPoint> > candidates;
  const double phases[] = {0.2, 0.5, 0.8};
  for(unsigned int angle=0; angle<180; angle+=30) {
    for(unsigned int phase=0; phase<3; phase++) {
      vector<XYPoint> route = buildSweepRoute(angle, phases[phase]);
      if(!route.empty()) {
        candidates.push_back(route);
        reverse(route.begin(), route.end());
        candidates.push_back(route);
      }
    }
  }

  double best = numeric_limits<double>::max();
  for(unsigned int i=0; i<candidates.size(); i++) {
    double score = routeScore(candidates[i]);
    if(score < best) {
      best = score;
      m_route = candidates[i];
    }
  }
  m_route_ix = 0;
  m_pt_set = false;
  postEventMessage("Built coverage pass " + uintToString(m_pass_count+1) +
                   " with " + uintToString(m_route.size()) +
                   " points; remembered trail points=" +
                   uintToString(m_route_history.size()));
}

vector<XYPoint> BHV_Scout::buildSweepRoute(double angle, double phase) const
{
  double radians = angle * M_PI / 180.0;
  double c = cos(radians);
  double s = sin(radians);
  double min_u = numeric_limits<double>::max();
  double max_u = -numeric_limits<double>::max();
  double min_v = numeric_limits<double>::max();
  double max_v = -numeric_limits<double>::max();

  for(unsigned int i=0; i<m_rescue_region.size(); i++) {
    double x = m_rescue_region.get_vx(i);
    double y = m_rescue_region.get_vy(i);
    double u = x*c + y*s;
    double v = -x*s + y*c;
    min_u = min(min_u, u);
    max_u = max(max_u, u);
    min_v = min(min_v, v);
    max_v = max(max_v, v);
  }

  vector<XYPoint> route;
  unsigned int lane = 0;
  double sample_spacing = min(m_lane_spacing, m_revisit_radius/2.0);
  sample_spacing = max(sample_spacing, 2.0);
  unsigned int samples =
    max(1u, (unsigned int)ceil((max_u-min_u)/sample_spacing));
  double sample_step = (max_u-min_u)/samples;

  for(double v=min_v + phase*m_lane_spacing;
      v<=max_v; v+=m_lane_spacing, lane++) {
    vector<XYPoint> row;
    for(unsigned int j=0; j<=samples; j++) {
      double u = min_u + j*sample_step;
      double x = u*c - v*s;
      double y = u*s + v*c;
      if(m_rescue_region.contains(x,y))
        row.push_back(XYPoint(x,y));
    }
    if(lane % 2)
      reverse(row.begin(), row.end());
    route.insert(route.end(), row.begin(), row.end());
  }
  return route;
}

double BHV_Scout::routeScore(const vector<XYPoint>& route) const
{
  if(route.empty())
    return numeric_limits<double>::max();
  double score = hypot(route[0].get_vx()-m_osx, route[0].get_vy()-m_osy);
  for(unsigned int i=1; i<route.size(); i++)
    score += hypot(route[i].get_vx()-route[i-1].get_vx(),
                   route[i].get_vy()-route[i-1].get_vy());
  score *= 0.05;

  if(!m_route_history.empty()) {
    double overlap = 0;
    for(unsigned int i=0; i<route.size(); i++) {
      double distance = distanceToHistory(route[i].get_vx(),
                                          route[i].get_vy());
      overlap += max(0.0, m_revisit_radius-distance);
    }
    score += m_revisit_weight * overlap / route.size();
  }

  double opponent = numeric_limits<double>::max();
  for(map<string,ScoutContact>::const_iterator p=m_contacts.begin(); p!=m_contacts.end(); ++p) {
    if((p->first == tolower(m_tmate)) || ((m_curr_time-p->second.time)>m_contact_timeout))
      continue;
    string type = tolower(p->second.type);
    if((type != "heron") && (type != "ship"))
      continue;
    opponent = min(opponent, hypot(route[0].get_vx()-p->second.x,
                                   route[0].get_vy()-p->second.y));
  }
  if(opponent < numeric_limits<double>::max())
    score += 0.15 * opponent;
  return score;
}

double BHV_Scout::distanceToHistory(double x, double y) const
{
  double best = numeric_limits<double>::max();
  for(unsigned int i=0; i<m_route_history.size(); i++)
    best = min(best, hypot(x-m_route_history[i].get_vx(),
                           y-m_route_history[i].get_vy()));
  return best;
}

void BHV_Scout::recordTrailPosition()
{
  if(!m_rescue_region.contains(m_osx, m_osy))
    return;
  if(m_route_history.empty() ||
     hypot(m_osx-m_route_history.back().get_vx(),
           m_osy-m_route_history.back().get_vy()) >= m_history_spacing)
    m_route_history.push_back(XYPoint(m_osx, m_osy));
}

void BHV_Scout::handleNodeReport(const string& report)
{
  string name = tolower(tokStringParse(report, "NAME", ',', '='));
  string type = tokStringParse(report, "TYPE", ',', '=');
  string xstr = tokStringParse(report, "X", ',', '=');
  string ystr = tokStringParse(report, "Y", ',', '=');
  if(name=="" || !isNumber(xstr) || !isNumber(ystr))
    return;
  ScoutContact contact;
  contact.x = atof(xstr.c_str());
  contact.y = atof(ystr.c_str());
  contact.time = m_curr_time;
  contact.type = type;
  m_contacts[name] = contact;
}

void BHV_Scout::postViewPoint(bool active)
{
  XYPoint point(m_ptx,m_pty);
  point.set_vertex_size(5);
  point.set_vertex_color("orange");
  point.set_label(m_us_name + "_next_scout_point");
  postMessage("VIEW_POINT", point.get_spec(active ? "active=true" : "active=false"));
}

void BHV_Scout::postViewRoute(bool active)
{
  XYSegList segl;
  for(unsigned int i=m_route_ix; i<m_route.size(); i++)
    segl.add_vertex(m_route[i].get_vx(), m_route[i].get_vy());
  segl.set_label(m_us_name + "_scout_route");
  segl.set_edge_color("orange");
  segl.set_vertex_color("yellow");
  segl.set_vertex_size(2);
  segl.set_active(active);
  postMessage("VIEW_SEGLIST", segl.get_spec());
}

IvPFunction *BHV_Scout::buildFunction()
{
  if(!m_pt_set)
    return 0;
  ZAIC_PEAK speed(m_domain,"speed");
  speed.setSummit(m_desired_speed);
  speed.setPeakWidth(0.2);
  speed.setBaseWidth(0.6);
  speed.setSummitDelta(0.8);
  if(!speed.stateOK())
    return 0;

  ZAIC_PEAK course(m_domain,"course");
  course.setSummit(relAng(m_osx,m_osy,m_ptx,m_pty));
  course.setPeakWidth(0);
  course.setBaseWidth(180);
  course.setSummitDelta(0);
  course.setValueWrap(true);
  if(!course.stateOK())
    return 0;

  OF_Coupler coupler;
  return coupler.couple(course.extractIvPFunction(), speed.extractIvPFunction(), 65, 35);
}

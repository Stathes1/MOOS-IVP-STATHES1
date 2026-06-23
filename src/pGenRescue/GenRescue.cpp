/************************************************************/
/*    FILE: GenRescue.cpp                                  */
/************************************************************/

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>
#include "ACTable.h"
#include "MBUtils.h"
#include "NodeRecord.h"
#include "NodeRecordUtils.h"
#include "XYSegList.h"
#include "GenRescue.h"

using namespace std;

GenRescue::GenRescue()
{
  m_got_nav_x = false;
  m_got_nav_y = false;
  m_nav_x = 0;
  m_nav_y = 0;

  m_update_var = "SURVEY_UPDATE";
  m_rescue_request_var = "RESCUE_REQUEST";
  m_planner = "two_step";
  m_use_adversary = true;
  m_request_range = 10.0;
  m_request_repeat = 4.0;
  m_path_refresh_interval = 2.0;
  m_last_path_post = 0;
  m_adversary_concede_range = 18.0;
  m_adversary_heading_gate = 65.0;
  m_adversary_stale_time = 8.0;
  m_lookahead_discount = 0.85;
  m_lookahead_depth = 3;
  m_concede_count = 1;
  m_updates_posted = 0;
  m_requests_posted = 0;
}

bool GenRescue::OnNewMail(MOOSMSG_LIST &NewMail)
{
  AppCastingMOOSApp::OnNewMail(NewMail);

  MOOSMSG_LIST::iterator p;
  for(p = NewMail.begin(); p != NewMail.end(); p++) {
    CMOOSMsg &msg = *p;
    string key = msg.GetKey();

    if((key == "SWIMMER_ALERT") && msg.IsString()) {
      if(handleSwimmerAlert(msg.GetString()))
        postPathUpdate();
    }
    else if((key == "FOUND_SWIMMER") && msg.IsString()) {
      if(handleFoundSwimmer(msg.GetString()))
        postPathUpdate();
    }
    else if((key == "NODE_REPORT") && msg.IsString()) {
      if(handleNodeReport(msg.GetString()))
        postPathUpdate();
    }
    else if((key == "NAV_X") && msg.IsDouble()) {
      bool had_nav = m_got_nav_x && m_got_nav_y;
      m_nav_x = msg.GetDouble();
      m_got_nav_x = true;
      if(!had_nav && m_got_nav_y && !m_swimmers.empty())
        postPathUpdate();
    }
    else if((key == "NAV_Y") && msg.IsDouble()) {
      bool had_nav = m_got_nav_x && m_got_nav_y;
      m_nav_y = msg.GetDouble();
      m_got_nav_y = true;
      if(!had_nav && m_got_nav_x && !m_swimmers.empty())
        postPathUpdate();
    }
    else if(key != "APPCAST_REQ")
      reportRunWarning("Unhandled Mail: " + key);
  }

  return(true);
}

bool GenRescue::OnConnectToServer()
{
  registerVariables();
  return(true);
}

bool GenRescue::Iterate()
{
  AppCastingMOOSApp::Iterate();
  postPeriodicPathUpdate();
  postRescueRequests();
  AppCastingMOOSApp::PostReport();
  return(true);
}

bool GenRescue::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for(p = sParams.begin(); p != sParams.end(); p++) {
    string orig = *p;
    string line = *p;
    string param = tolower(biteStringX(line, '='));
    string value = stripBlankEnds(line);

    bool handled = false;
    if((param == "update_var") && !strContainsWhite(value) && (value != "")) {
      m_update_var = value;
      handled = true;
    }
    else if((param == "rescue_request_var") &&
            !strContainsWhite(value) && (value != "")) {
      m_rescue_request_var = value;
      handled = true;
    }
    else if((param == "planner") && (value != "")) {
      m_planner = tolower(value);
      handled = true;
    }
    else if((param == "use_adversary") && (value != "")) {
      string v = tolower(value);
      m_use_adversary = ((v == "true") || (v == "on") ||
                         (v == "yes") || (v == "1"));
      handled = true;
    }
    else if((param == "request_range") && isNumber(value) &&
            (atof(value.c_str()) > 0)) {
      m_request_range = atof(value.c_str());
      handled = true;
    }
    else if((param == "request_repeat") && isNumber(value) &&
            (atof(value.c_str()) > 0)) {
      m_request_repeat = atof(value.c_str());
      handled = true;
    }
    else if((param == "path_refresh_interval") && isNumber(value) &&
            (atof(value.c_str()) > 0)) {
      m_path_refresh_interval = atof(value.c_str());
      handled = true;
    }
    else if((param == "adversary_concede_range") && isNumber(value) &&
            (atof(value.c_str()) >= 0)) {
      m_adversary_concede_range = atof(value.c_str());
      handled = true;
    }
    else if((param == "adversary_heading_gate") && isNumber(value) &&
            (atof(value.c_str()) >= 0)) {
      m_adversary_heading_gate = atof(value.c_str());
      handled = true;
    }
    else if((param == "adversary_stale_time") && isNumber(value) &&
            (atof(value.c_str()) > 0)) {
      m_adversary_stale_time = atof(value.c_str());
      handled = true;
    }
    else if((param == "lookahead_discount") && isNumber(value) &&
            (atof(value.c_str()) >= 0)) {
      m_lookahead_discount = atof(value.c_str());
      handled = true;
    }
    else if((param == "lookahead_depth") && isNumber(value) &&
            (atoi(value.c_str()) > 0)) {
      m_lookahead_depth = (unsigned int)atoi(value.c_str());
      handled = true;
    }
    else if((param == "cluster_weight") && isNumber(value) &&
            (atof(value.c_str()) >= 0)) {
      m_lookahead_discount = atof(value.c_str());
      handled = true;
    }
    else if((param == "concede_count") && isNumber(value) &&
            (atoi(value.c_str()) >= 0)) {
      m_concede_count = (unsigned int)atoi(value.c_str());
      handled = true;
    }
    else if(param == "vname") {
      m_vname = value;
      handled = true;
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);
  }

  if(m_vname == "")
    m_vname = m_Comms.GetCommunityName();

  registerVariables();
  return(true);
}

void GenRescue::registerVariables()
{
  AppCastingMOOSApp::RegisterVariables();
  Register("SWIMMER_ALERT", 0);
  Register("FOUND_SWIMMER", 0);
  Register("NODE_REPORT", 0);
  Register("NAV_X", 0);
  Register("NAV_Y", 0);
}

bool GenRescue::handleSwimmerAlert(const string& alert)
{
  string id = tokStringParse(alert, "id", ',', '=');
  string xstr = tokStringParse(alert, "x", ',', '=');
  string ystr = tokStringParse(alert, "y", ',', '=');

  id = stripBlankEnds(id);
  if((id == "") || !isNumber(xstr) || !isNumber(ystr)) {
    reportRunWarning("Bad SWIMMER_ALERT: " + alert);
    return(false);
  }

  if(m_swimmers.count(id) != 0)
    return(false);

  Swimmer swimmer;
  swimmer.id = id;
  swimmer.x = atof(xstr.c_str());
  swimmer.y = atof(ystr.c_str());
  swimmer.rescued = (m_found_ids.count(id) != 0);
  m_swimmers[id] = swimmer;
  return(true);
}

bool GenRescue::handleFoundSwimmer(const string& report)
{
  string id = stripBlankEnds(tokStringParse(report, "id", ',', '='));
  if(id == "") {
    reportRunWarning("Bad FOUND_SWIMMER: " + report);
    return(false);
  }

  if(m_found_ids.count(id) != 0)
    return(false);

  m_found_ids.insert(id);
  if(m_swimmers.count(id) != 0)
    m_swimmers[id].rescued = true;

  return(true);
}

bool GenRescue::handleNodeReport(const string& report)
{
  NodeRecord record = string2NodeRecord(report);
  if(!record.valid())
    return(false);

  string contact_name = record.getName();
  if((contact_name == "") || (contact_name == m_vname))
    return(false);

  bool changed = (!m_contact.valid) ||
                 (distTo(m_contact.x, m_contact.y, record.getX(), record.getY()) > 2.0) ||
                 (m_contact.name != contact_name);

  m_contact.name = contact_name;
  m_contact.x = record.getX();
  m_contact.y = record.getY();
  m_contact.heading = record.getHeading();
  m_contact.timestamp = MOOSTime();
  m_contact.valid = true;
  return(changed);
}

void GenRescue::postPathUpdate()
{
  m_last_path_post = MOOSTime();
  vector<string> order = buildPathOrder();

  XYSegList segl;
  if(order.empty()) {
    if(!m_got_nav_x || !m_got_nav_y)
      return;
    segl.add_vertex(m_nav_x, m_nav_y);
  }
  else {
    for(unsigned int i = 0; i < order.size(); i++) {
      const Swimmer& swimmer = m_swimmers[order[i]];
      segl.add_vertex(swimmer.x, swimmer.y);
    }
  }

  Notify(m_update_var, "points=" + segl.get_spec_pts(1));
  m_updates_posted++;
}

void GenRescue::postPeriodicPathUpdate()
{
  if(!m_got_nav_x || !m_got_nav_y || m_swimmers.empty())
    return;
  if(getAvailableSwimmers().empty())
    return;

  if((MOOSTime() - m_last_path_post) < m_path_refresh_interval)
    return;

  postPathUpdate();
}

void GenRescue::postRescueRequests()
{
  if(!m_got_nav_x || !m_got_nav_y)
    return;

  double now = MOOSTime();
  map<string, Swimmer>::iterator p;
  for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
    Swimmer& swimmer = p->second;
    if(swimmer.rescued)
      continue;

    if(distTo(m_nav_x, m_nav_y, swimmer.x, swimmer.y) > m_request_range)
      continue;

    if((m_last_request_time.count(swimmer.id) != 0) &&
       ((now - m_last_request_time[swimmer.id]) < m_request_repeat))
      continue;

    string request = "id=" + swimmer.id;
    if(m_vname != "")
      request += ",vname=" + m_vname;

    Notify(m_rescue_request_var, request);
    m_last_request_time[swimmer.id] = now;
    m_requests_posted++;
  }
}

vector<string> GenRescue::getAvailableSwimmers() const
{
  set<string> conceded = getConcededSwimmers();
  vector<string> remaining;
  map<string, Swimmer>::const_iterator p;
  for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
    if(p->second.rescued)
      continue;
    if(conceded.count(p->first) != 0)
      continue;
    remaining.push_back(p->first);
  }

  return(remaining);
}

set<string> GenRescue::getConcededSwimmers() const
{
  set<string> conceded;
  if(!m_use_adversary || !contactIsFresh() || (m_concede_count == 0))
    return(conceded);

  vector<pair<double, string> > ranked;
  map<string, Swimmer>::const_iterator p;
  for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
    const Swimmer& swimmer = p->second;
    if(swimmer.rescued || !shouldConcede(swimmer))
      continue;

    double contact_dist = distTo(m_contact.x, m_contact.y, swimmer.x, swimmer.y);
    double bearing = fabs(relBearing(m_contact.x, m_contact.y, m_contact.heading,
                                     swimmer.x, swimmer.y));
    ranked.push_back(pair<double, string>(contact_dist + (0.1 * bearing), p->first));
  }

  sort(ranked.begin(), ranked.end());
  unsigned int amt = ranked.size();
  if(amt > m_concede_count)
    amt = m_concede_count;

  for(unsigned int i = 0; i < amt; i++)
    conceded.insert(ranked[i].second);

  return(conceded);
}

vector<string> GenRescue::buildPathOrder() const
{
  vector<string> remaining = getAvailableSwimmers();
  vector<string> ordered;
  double cx = m_got_nav_x ? m_nav_x : 0;
  double cy = m_got_nav_y ? m_nav_y : 0;

  while(!remaining.empty()) {
    unsigned int best_ix = 0;
    double best_score = numeric_limits<double>::max();

    for(unsigned int i = 0; i < remaining.size(); i++) {
      double score = candidateScore(remaining[i], remaining, cx, cy);
      if(score < best_score) {
        best_score = score;
        best_ix = i;
      }
    }

    string next_id = remaining[best_ix];
    const Swimmer& next_swimmer = m_swimmers.find(next_id)->second;
    ordered.push_back(next_id);
    cx = next_swimmer.x;
    cy = next_swimmer.y;
    remaining.erase(remaining.begin() + best_ix);
  }

  return(ordered);
}

bool GenRescue::shouldConcede(const Swimmer& swimmer) const
{
  if(!m_use_adversary || !contactIsFresh())
    return(false);

  double contact_dist = distTo(m_contact.x, m_contact.y, swimmer.x, swimmer.y);
  double own_dist = (m_got_nav_x && m_got_nav_y) ?
    distTo(m_nav_x, m_nav_y, swimmer.x, swimmer.y) : numeric_limits<double>::max();

  if((contact_dist <= m_adversary_concede_range) && (contact_dist + 5.0 < own_dist))
    return(true);

  double bearing = relBearing(m_contact.x, m_contact.y, m_contact.heading,
                              swimmer.x, swimmer.y);
  if((contact_dist < own_dist) && (fabs(bearing) <= m_adversary_heading_gate))
    return(true);

  return(false);
}

bool GenRescue::contactIsFresh() const
{
  return(m_contact.valid && ((MOOSTime() - m_contact.timestamp) <= m_adversary_stale_time));
}

double GenRescue::candidateScore(const string& id,
                                 const vector<string>& remaining,
                                 double cx, double cy) const
{
  const Swimmer& swimmer = m_swimmers.find(id)->second;
  double leg_dist = distTo(cx, cy, swimmer.x, swimmer.y);

  if(m_planner == "greedy")
    return(leg_dist);

  vector<string> next_remaining = remaining;
  vector<string>::iterator p = find(next_remaining.begin(), next_remaining.end(), id);
  if(p != next_remaining.end())
    next_remaining.erase(p);

  if(next_remaining.empty())
    return(leg_dist);

  unsigned int depth = m_lookahead_depth;
  if(m_planner == "two_step")
    depth = 2;

  double future_cost = routeLookaheadCost(next_remaining, swimmer.x, swimmer.y,
                                          depth - 1);
  return(leg_dist + (m_lookahead_discount * future_cost));
}

double GenRescue::routeLookaheadCost(vector<string> remaining,
                                     double cx, double cy,
                                     unsigned int depth) const
{
  if(remaining.empty() || (depth == 0))
    return(0);

  double best = numeric_limits<double>::max();
  for(unsigned int i = 0; i < remaining.size(); i++) {
    const Swimmer& swimmer = m_swimmers.find(remaining[i])->second;
    double leg_dist = distTo(cx, cy, swimmer.x, swimmer.y);

    vector<string> next_remaining = remaining;
    next_remaining.erase(next_remaining.begin() + i);

    double score = leg_dist +
      (m_lookahead_discount *
       routeLookaheadCost(next_remaining, swimmer.x, swimmer.y, depth - 1));

    if(score < best)
      best = score;
  }

  return(best);
}

double GenRescue::distTo(double x, double y, double px, double py) const
{
  double dx = x - px;
  double dy = y - py;
  return(sqrt((dx * dx) + (dy * dy)));
}

double GenRescue::relBearing(double from_x, double from_y, double heading,
                             double to_x, double to_y) const
{
  double angle = atan2((to_y - from_y), (to_x - from_x)) * 180.0 / M_PI;
  angle = 90.0 - angle;
  while(angle < 0)
    angle += 360.0;
  while(angle >= 360)
    angle -= 360.0;

  double rel = angle - heading;
  while(rel > 180)
    rel -= 360;
  while(rel < -180)
    rel += 360;
  return(rel);
}

bool GenRescue::buildReport()
{
  set<string> conceded_ids = getConcededSwimmers();
  unsigned int active = 0;
  unsigned int conceded = 0;
  map<string, Swimmer>::const_iterator p;
  for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
    if(p->second.rescued)
      continue;
    if(conceded_ids.count(p->first) != 0)
      conceded++;
    else
      active++;
  }

  m_msgs << "Known swimmers: " << m_swimmers.size() << endl;
  m_msgs << "Active swimmers: " << active << endl;
  m_msgs << "Conceded swimmers: " << conceded << endl;
  m_msgs << "Path updates posted: " << m_updates_posted << endl;
  m_msgs << "Rescue requests posted: " << m_requests_posted << endl;
  m_msgs << "Planner: " << m_planner << endl;
  m_msgs << "Lookahead depth: " << m_lookahead_depth << endl;
  m_msgs << "Lookahead discount: " << m_lookahead_discount << endl;
  m_msgs << "Request range: " << m_request_range << endl;
  m_msgs << "Path refresh interval: " << m_path_refresh_interval << endl;
  if(contactIsFresh())
    m_msgs << "Adversary: " << m_contact.name << " at "
           << doubleToStringX(m_contact.x, 1) << ","
           << doubleToStringX(m_contact.y, 1) << " hdg="
           << doubleToStringX(m_contact.heading, 0) << endl;
  else
    m_msgs << "Adversary: n/a" << endl;
  m_msgs << endl;

  ACTable actab(5);
  actab << "ID | X | Y | State | Concede";
  actab.addHeaderLines();

  for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
    const Swimmer& swimmer = p->second;
    string state = swimmer.rescued ? "rescued" : "open";
    actab << swimmer.id
          << doubleToStringX(swimmer.x, 1)
          << doubleToStringX(swimmer.y, 1)
          << state
          << boolToString(conceded_ids.count(p->first) != 0);
  }

  m_msgs << actab.getFormattedString();
  return(true);
}

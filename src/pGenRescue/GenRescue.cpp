/************************************************************/
/*    FILE: GenRescue.cpp                                  */
/************************************************************/

#include <cmath>
#include <limits>
#include "ACTable.h"
#include "MBUtils.h"
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
  m_request_range = 7.0;
  m_request_repeat = 4.0;
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

void GenRescue::postPathUpdate()
{
  vector<string> order = buildGreedyOrder();

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

vector<string> GenRescue::buildGreedyOrder() const
{
  vector<string> remaining;
  map<string, Swimmer>::const_iterator p;
  for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
    if(!p->second.rescued)
      remaining.push_back(p->first);
  }

  vector<string> ordered;
  double cx = m_got_nav_x ? m_nav_x : 0;
  double cy = m_got_nav_y ? m_nav_y : 0;

  while(!remaining.empty()) {
    unsigned int best_ix = 0;
    double best_dist = numeric_limits<double>::max();

    for(unsigned int i = 0; i < remaining.size(); i++) {
      const Swimmer& swimmer = m_swimmers.find(remaining[i])->second;
      double dist = distTo(cx, cy, swimmer.x, swimmer.y);
      if(dist < best_dist) {
        best_dist = dist;
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

double GenRescue::distTo(double x, double y, double px, double py) const
{
  double dx = x - px;
  double dy = y - py;
  return(sqrt((dx * dx) + (dy * dy)));
}

bool GenRescue::buildReport()
{
  m_msgs << "Known swimmers: " << m_swimmers.size() << endl;
  m_msgs << "Path updates posted: " << m_updates_posted << endl;
  m_msgs << "Rescue requests posted: " << m_requests_posted << endl;
  m_msgs << "Request range: " << m_request_range << endl;
  m_msgs << endl;

  ACTable actab(4);
  actab << "ID | X | Y | Rescued";
  actab.addHeaderLines();

  map<string, Swimmer>::const_iterator p;
  for(p = m_swimmers.begin(); p != m_swimmers.end(); p++) {
    const Swimmer& swimmer = p->second;
    actab << swimmer.id
          << doubleToStringX(swimmer.x, 1)
          << doubleToStringX(swimmer.y, 1)
          << boolToString(swimmer.rescued);
  }

  m_msgs << actab.getFormattedString();
  return(true);
}

/************************************************************/
/*    FILE: BHV_Pulse.cpp                                  */
/************************************************************/

#include <cmath>
#include <cstdlib>
#include "BHV_Pulse.h"
#include "MBUtils.h"
#include "XYRangePulse.h"

using namespace std;

BHV_Pulse::BHV_Pulse(IvPDomain gdomain) : IvPBehavior(gdomain)
{
  IvPBehavior::setParam("name", "pulse");

  m_pulse_range = 20;
  m_pulse_duration = 4;
  m_pulse_delay = 5;
  m_edge_color = "yellow";
  m_fill_color = "yellow";

  m_osx = 0;
  m_osy = 0;
  m_curr_time = 0;
  m_last_wpt_index = -1;
  m_event_count = 0;
  m_pending_wpt_index = -1;
  m_pending_time = 0;
  m_pending_pulse = false;

  m_fallback_ix = 0;
  m_capture_radius = 6;
  m_pts_x.push_back(60);  m_pts_y.push_back(-40);
  m_pts_x.push_back(60);  m_pts_y.push_back(-160);
  m_pts_x.push_back(150); m_pts_y.push_back(-160);
  m_pts_x.push_back(180); m_pts_y.push_back(-100);
  m_pts_x.push_back(150); m_pts_y.push_back(-40);

  addInfoVars("NAV_X, NAV_Y");
  addInfoVars("WPT_INDEX, NEXT", "no_warning");
}

bool BHV_Pulse::setParam(string param, string val)
{
  param = tolower(param);
  double dval = atof(val.c_str());

  if((param == "pulse_range") && isNumber(val) && (dval > 0)) {
    m_pulse_range = dval;
    return(true);
  }
  if((param == "pulse_duration") && isNumber(val) && (dval > 0)) {
    m_pulse_duration = dval;
    return(true);
  }
  if((param == "pulse_delay") && isNumber(val) && (dval >= 0)) {
    m_pulse_delay = dval;
    return(true);
  }
  if((param == "capture_radius") && isNumber(val) && (dval > 0)) {
    m_capture_radius = dval;
    return(true);
  }
  if((param == "edge_color") && (val != "")) {
    m_edge_color = val;
    return(true);
  }
  if((param == "fill_color") && (val != "")) {
    m_fill_color = val;
    return(true);
  }

  return(false);
}

IvPFunction* BHV_Pulse::onRunState()
{
  if(!updateInfo())
    return(0);

  int event_id = 0;
  if(detectWaypointEvent(event_id))
    handleWaypointEvent(event_id);

  if(m_pending_pulse && (m_curr_time >= m_pending_time)) {
    postPulse();
    m_pending_pulse = false;  }

  return(0);
}

bool BHV_Pulse::updateInfo()
{
  bool ok1 = false;
  bool ok2 = false;
  m_osx = getBufferDoubleVal("NAV_X", ok1);
  m_osy = getBufferDoubleVal("NAV_Y", ok2);
  m_curr_time = getBufferCurrTime();

  if(!ok1 || !ok2) {
    postWMessage("No ownship X/Y info in info_buffer.");
    return(false);
  }
  return(true);
}

bool BHV_Pulse::detectWaypointEvent(int& event_id)
{
  bool ok_ix = false;
  double wpt_index = getBufferDoubleVal("WPT_INDEX", ok_ix);
  if(ok_ix) {
    int index = (int)(wpt_index + 0.5);
    if(m_last_wpt_index < 0)
      m_last_wpt_index = index;
    else if(index != m_last_wpt_index) {
      m_last_wpt_index = index;
      m_event_count++;
      event_id = index;
      return(true);
    }
  }

  bool ok_next = false;
  string next_wpt = getBufferStringVal("NEXT", ok_next);
  if(ok_next && (next_wpt != "")) {
    if(m_last_next_wpt == "")
      m_last_next_wpt = next_wpt;
    else if(next_wpt != m_last_next_wpt) {
      m_last_next_wpt = next_wpt;
      m_event_count++;
      event_id = m_event_count;
      return(true);
    }
  }

  if(m_fallback_ix < m_pts_x.size()) {
    double dx = m_osx - m_pts_x[m_fallback_ix];
    double dy = m_osy - m_pts_y[m_fallback_ix];
    if(hypot(dx, dy) <= m_capture_radius) {
      m_fallback_ix++;
      m_event_count++;
      event_id = m_event_count;
      return(true);
    }
  }

  return(false);
}

void BHV_Pulse::handleWaypointEvent(int event_id)
{
  m_pending_wpt_index = event_id;
  m_pending_time = m_curr_time + m_pulse_delay;
  m_pending_pulse = true;
}

void BHV_Pulse::postPulse()
{
  XYRangePulse pulse;
  pulse.set_x(m_osx);
  pulse.set_y(m_osy);
  pulse.set_label("pulse_" + intToString(m_pending_wpt_index));
  pulse.set_rad(m_pulse_range);
  pulse.set_duration(m_pulse_duration);
  pulse.set_time(m_curr_time);
  pulse.set_color("edge", m_edge_color);
  pulse.set_color("fill", m_fill_color);
  pulse.set_edge_size(1);

  postMessage("VIEW_RANGE_PULSE", pulse.get_spec());
}

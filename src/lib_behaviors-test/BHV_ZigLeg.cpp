/************************************************************/
/*    FILE: BHV_ZigLeg.cpp                                */
/************************************************************/

#include <cmath>
#include <cstdlib>
#include "BHV_ZigLeg.h"
#include "BuildUtils.h"
#include "AngleUtils.h"
#include "MBUtils.h"
#include "XYRangePulse.h"
#include "ZAIC_PEAK.h"

using namespace std;

BHV_ZigLeg::BHV_ZigLeg(IvPDomain gdomain) : IvPBehavior(gdomain)
{
  IvPBehavior::setParam("name", "zigleg");
  m_domain = subDomain(m_domain, "course");

  m_pulse_range = 20;
  m_pulse_duration = 4;
  m_pulse_delay = 5;
  m_zig_duration = 10;
  m_zig_angle = 45;
  m_edge_color = "yellow";
  m_fill_color = "yellow";

  m_osx = 0;
  m_osy = 0;
  m_osh = 0;
  m_curr_time = 0;
  m_last_wpt_index = -1;
  m_event_count = 0;
  m_pending_wpt_index = -1;
  m_pending_time = 0;
  m_pending_zig = false;
  m_zig_active = false;
  m_zig_heading = 0;
  m_zig_end_time = 0;

  m_fallback_ix = 0;
  m_capture_radius = 6;
  m_pts_x.push_back(60);  m_pts_y.push_back(-40);
  m_pts_x.push_back(60);  m_pts_y.push_back(-160);
  m_pts_x.push_back(150); m_pts_y.push_back(-160);
  m_pts_x.push_back(180); m_pts_y.push_back(-100);
  m_pts_x.push_back(150); m_pts_y.push_back(-40);

  addInfoVars("NAV_X, NAV_Y, NAV_HEADING");
  addInfoVars("WPT_INDEX, NEXT", "no_warning");
}

bool BHV_ZigLeg::setParam(string param, string val)
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
  if((param == "zig_duration") && isNumber(val) && (dval > 0)) {
    m_zig_duration = dval;
    return(true);
  }
  if((param == "zig_angle") && isNumber(val)) {
    m_zig_angle = dval;
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

IvPFunction* BHV_ZigLeg::onRunState()
{
  if(!updateInfo())
    return(0);

  int event_id = 0;
  if(detectWaypointEvent(event_id))
    handleWaypointEvent(event_id);

  if(m_pending_zig && (m_curr_time >= m_pending_time))
    startZig();

  if(m_zig_active && (m_curr_time > m_zig_end_time))
    m_zig_active = false;

  if(!m_zig_active)
    return(0);

  return(buildZigOF());
}

bool BHV_ZigLeg::updateInfo()
{
  bool ok1 = false;
  bool ok2 = false;
  bool ok3 = false;
  m_osx = getBufferDoubleVal("NAV_X", ok1);
  m_osy = getBufferDoubleVal("NAV_Y", ok2);
  m_osh = getBufferDoubleVal("NAV_HEADING", ok3);
  m_curr_time = getBufferCurrTime();

  if(!ok1 || !ok2 || !ok3) {
    postWMessage("No ownship X/Y/heading info in info_buffer.");
    return(false);
  }
  return(true);
}

bool BHV_ZigLeg::detectWaypointEvent(int& event_id)
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

void BHV_ZigLeg::handleWaypointEvent(int event_id)
{
  m_pending_wpt_index = event_id;
  m_pending_time = m_curr_time + m_pulse_delay;
  m_pending_zig = true;
}

void BHV_ZigLeg::startZig()
{
  postPulse();
  m_zig_heading = angle360(m_osh + m_zig_angle);
  m_zig_end_time = m_curr_time + m_zig_duration;
  m_zig_active = true;
  m_pending_zig = false;
}

void BHV_ZigLeg::postPulse()
{
  XYRangePulse pulse;
  pulse.set_x(m_osx);
  pulse.set_y(m_osy);
  pulse.set_label("zig_" + intToString(m_pending_wpt_index));
  pulse.set_rad(m_pulse_range);
  pulse.set_duration(m_pulse_duration);
  pulse.set_time(m_curr_time);
  pulse.set_color("edge", m_edge_color);
  pulse.set_color("fill", m_fill_color);
  pulse.set_edge_size(1);

  postMessage("VIEW_RANGE_PULSE", pulse.get_spec());
}

IvPFunction* BHV_ZigLeg::buildZigOF()
{
  ZAIC_PEAK crs_zaic(m_domain, "course");
  crs_zaic.setSummit(m_zig_heading);
  crs_zaic.setPeakWidth(0);
  crs_zaic.setBaseWidth(180);
  crs_zaic.setSummitDelta(0);
  crs_zaic.setValueWrap(true);

  if(!crs_zaic.stateOK()) {
    postWMessage("Course ZAIC problems " + crs_zaic.getWarnings());
    return(0);
  }

  IvPFunction *ipf = crs_zaic.extractIvPFunction();
  if(ipf)
    ipf->setPWT(m_priority_wt);
  return(ipf);
}

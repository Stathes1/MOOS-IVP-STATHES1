/************************************************************/
/*    FILE: BHV_Pulse.h                                    */
/************************************************************/

#ifndef BHV_PULSE_HEADER
#define BHV_PULSE_HEADER

#include <string>
#include <vector>
#include "IvPBehavior.h"

class BHV_Pulse : public IvPBehavior {
public:
  BHV_Pulse(IvPDomain);
  ~BHV_Pulse() {}

  bool         setParam(std::string, std::string);
  IvPFunction* onRunState();

protected:
  bool         updateInfo();
  bool         detectWaypointEvent(int& event_id);
  void         handleWaypointEvent(int event_id);
  void         postPulse();

protected: // Configuration parameters
  double       m_pulse_range;
  double       m_pulse_duration;
  double       m_pulse_delay;
  std::string  m_edge_color;
  std::string  m_fill_color;

protected: // State variables
  double       m_osx;
  double       m_osy;
  double       m_curr_time;
  int          m_last_wpt_index;
  int          m_event_count;
  int          m_pending_wpt_index;
  double       m_pending_time;
  bool         m_pending_pulse;
  std::string  m_last_next_wpt;
  unsigned int m_fallback_ix;
  double       m_capture_radius;
  std::vector<double> m_pts_x;
  std::vector<double> m_pts_y;
};

#ifdef WIN32
  #define IVP_EXPORT_FUNCTION __declspec(dllexport)
#else
  #define IVP_EXPORT_FUNCTION
#endif

extern "C" {
  IVP_EXPORT_FUNCTION IvPBehavior * createBehavior(std::string name, IvPDomain domain)
  {return new BHV_Pulse(domain);}
}
#endif

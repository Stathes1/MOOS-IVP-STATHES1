/************************************************************/
/*    NAME: Efstathios Raptis                                              */
/*    ORGN: MIT, Cambridge MA                               */
/*    FILE: Odometry.cpp                                        */
/*    DATE: June 16th, 2026                             */
/************************************************************/

#include <cmath>
#include <iterator>
#include "MBUtils.h"
#include "ACTable.h"
#include "Odometry.h"

using namespace std;

//---------------------------------------------------------
// Constructor()

Odometry::Odometry()
{m_first_reading = true;
m_got_x = false;
m_got_y = false;
m_current_x = 0;
m_current_y = 0;
m_previous_x = 0;
m_previous_y = 0;
m_total_distance = 0;
}

//---------------------------------------------------------
// Destructor

Odometry::~Odometry()
{
}

//---------------------------------------------------------
// Procedure: OnNewMail()

bool Odometry::OnNewMail(MOOSMSG_LIST &NewMail)
{
AppCastingMOOSApp::OnNewMail(NewMail);
MOOSMSG_LIST::iterator p;
for(p = NewMail.begin(); p != NewMail.end(); p++) {
CMOOSMsg &msg = *p;
std::string key = msg.GetKey();
if(key == "NAV_X" && msg.IsDouble()) {
m_current_x = msg.GetDouble();
m_got_x = true;
}
else if(key == "NAV_Y" && msg.IsDouble()) {
m_current_y = msg.GetDouble();
m_got_y = true;
}
else {
reportRunWarning("Unhandled Mail: " + key);
}
if(m_got_x && m_got_y) {
if(m_first_reading) {
m_previous_x = m_current_x;
m_previous_y = m_current_y;
m_first_reading = false;
}
else {
double dx = m_current_x - m_previous_x;
double dy = m_current_y - m_previous_y;
double leg_dist = sqrt((dx * dx) + (dy * dy));
m_total_distance += leg_dist;
m_previous_x = m_current_x;
m_previous_y = m_current_y;
}
}
}
return(true);
}
//---------------------------------------------------------
// Procedure: OnConnectToServer()

bool Odometry::OnConnectToServer()
{
   registerVariables();
   return(true);
}

//---------------------------------------------------------
// Procedure: Iterate()
//            happens AppTick times per second

bool Odometry::Iterate()
{
  AppCastingMOOSApp::Iterate();
  Notify("ODOMETRY_DIST", m_total_distance);
  AppCastingMOOSApp::PostReport();
  return(true);
}

//---------------------------------------------------------
// Procedure: OnStartUp()
//            happens before connection is open

bool Odometry::OnStartUp()
{
  AppCastingMOOSApp::OnStartUp();

  STRING_LIST sParams;
  m_MissionReader.EnableVerbatimQuoting(false);
  if(!m_MissionReader.GetConfiguration(GetAppName(), sParams))
    reportConfigWarning("No config block found for " + GetAppName());

  STRING_LIST::iterator p;
  for(p=sParams.begin(); p!=sParams.end(); p++) {
    string orig  = *p;
    string line  = *p;
    string param = tolower(biteStringX(line, '='));
    string value = line;

    bool handled = false;
    if(param == "foo") {
      handled = true;
    }
    else if(param == "bar") {
      handled = true;
    }

    if(!handled)
      reportUnhandledConfigWarning(orig);

  }
  
  registerVariables();	
  return(true);
}

//---------------------------------------------------------
// Procedure: registerVariables()

void Odometry::registerVariables()
{
AppCastingMOOSApp::RegisterVariables();
Register("NAV_X", 0);
Register("NAV_Y", 0);
}

//------------------------------------------------------------
// Procedure: buildReport()

bool Odometry::buildReport() 
{
  m_msgs << "============================================" << endl;
  m_msgs << "File:                                       " << endl;
  m_msgs << "============================================" << endl;

  ACTable actab(4);
  actab << "Alpha | Bravo | Charlie | Delta";
  actab.addHeaderLines();
  actab << "one" << "two" << "three" << "four";
  m_msgs << actab.getFormattedString();

  return(true);
}





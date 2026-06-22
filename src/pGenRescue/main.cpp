/************************************************************/
/*    FILE: main.cpp                                       */
/************************************************************/

#include <cstdlib>
#include <iostream>
#include <string>
#include "MBUtils.h"
#include "ColorParse.h"
#include "GenRescue.h"

using namespace std;

void showHelpAndExit()
{
  cout << "Usage: pGenRescue file.moos [OPTIONS]                         " << endl;
  cout << "                                                               " << endl;
  cout << "Options:                                                       " << endl;
  cout << "  --alias=<ProcessName>                                        " << endl;
  cout << "  -h,--help                                                    " << endl;
  cout << "                                                               " << endl;
  exit(0);
}

int main(int argc, char *argv[])
{
  string mission_file;
  string run_command = argv[0];

  for(int i = 1; i < argc; i++) {
    string argi = argv[i];
    if((argi == "-h") || (argi == "--help") || (argi == "-help"))
      showHelpAndExit();
    else if(strEnds(argi, ".moos") || strEnds(argi, ".moos++"))
      mission_file = argv[i];
    else if(strBegins(argi, "--alias="))
      run_command = argi.substr(8);
    else if(i == 2)
      run_command = argi;
  }

  if(mission_file == "")
    showHelpAndExit();

  cout << termColor("green");
  cout << "pGenRescue launching as " << run_command << endl;
  cout << termColor() << endl;

  GenRescue app;
  app.Run(run_command.c_str(), mission_file.c_str());

  return(0);
}

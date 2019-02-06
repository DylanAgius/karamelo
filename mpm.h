/* -*- c++ -*- ----------------------------------------------------------*/

#ifndef MPM_MPM_H
#define MPM_MPM_H

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <fstream>
#include <map>

using namespace std;

class MPM {

 public:

  class Input *input;            // input script processing
  class Output *output;          // thermo/dump/restart

  class Domain *domain;          // simulation box
  class Material *material;      // material
  class Update *update;          //
  class Modify *modify;          // fixes and computes

  filebuf infile;                // infile
  filebuf logfile;               // logfile

  map<string, double> variables; // global variables

  MPM(int, char **);
  ~MPM();
};

#endif

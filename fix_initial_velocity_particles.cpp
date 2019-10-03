#include <iostream>
#include <vector>
#include <string>
#include <Eigen/Eigen>
#include "fix_initial_velocity_particles.h"
#include "input.h"
#include "group.h"
#include "domain.h"
#include "update.h"
#include "solid.h"
#include "error.h"

using namespace std;
using namespace FixConst;
using namespace Eigen;

FixInitialVelocityParticles::FixInitialVelocityParticles(MPM *mpm, vector<string> args) : Fix(mpm, args)
{
  if (args.size() < 6) {
    error->all(FLERR,"Error: too few arguments for fix_initial_velocity_particles: requires at least 6 arguments. " + to_string(args.size()) + " received.\n");
  }

  if (group->pon[igroup].compare("particles") !=0 && group->pon[igroup].compare("all") !=0) {
    error->all(FLERR, "fix_initial_velocity_particles needs to be given a group of particles" + group->pon[igroup] + ", " + args[2] + " is a group of " + group->pon[igroup] + ".\n");
  }
  cout << "Creating new fix FixInitialVelocityParticles with ID: " << args[0] << endl;
  id = args[0];

  xset = yset = zset = false;

  if (args[3].compare("NULL") != 0) {
    xvalue = input->parsev(args[3]);
    xset = true;
  }

  if (args[4].compare("NULL") != 0) {
    yvalue = input->parsev(args[4]);
    yset = true;
  }

  if (args[5].compare("NULL") != 0) {
    zvalue = input->parsev(args[5]);
    zset = true;
  }
}

FixInitialVelocityParticles::~FixInitialVelocityParticles()
{
}

void FixInitialVelocityParticles::init()
{
}

void FixInitialVelocityParticles::setup()
{
}

void FixInitialVelocityParticles::setmask() {
  mask = 0;
  mask |= INITIAL_INTEGRATE;
}


void FixInitialVelocityParticles::initial_integrate() {
  if (update->ntimestep !=1) return;
  cout << "In FixInitialVelocityParticles::initial_integrate()" << endl;

  // Go through all the particles in the group and set v to the right value:
  double vx, vy, vz;
  
  int solid = group->solid[igroup];

  Solid *s;

  if (solid == -1) {
    for (int isolid = 0; isolid < domain->solids.size(); isolid++) {
      s = domain->solids[isolid];

      for (int ip = 0; ip < s->np_local; ip++) {
	if (s->mask[ip] & groupbit) {
	  (*input->vars)["x"] = Var("x", s->x[ip][0]);
	  (*input->vars)["y"] = Var("y", s->x[ip][1]);
	  (*input->vars)["z"] = Var("z", s->x[ip][2]);
	  if (xset) {
	    vx = xvalue.result(mpm);
	    s->v[ip][0] = vx;
	  }
	  if (yset) {
	    vy = yvalue.result(mpm);
	    s->v[ip][1] = vy;
	  }
	  if (zset) {
	    vz = zvalue.result(mpm);
	    s->v[ip][2] = vz;
	  }
	}
      }
    }
  } else {
    s = domain->solids[solid];

    for (int ip = 0; ip < s->np_local; ip++) {
      if (s->mask[ip] & groupbit) {
	(*input->vars)["x"] = Var("x", s->x[ip][0]);
	(*input->vars)["y"] = Var("y", s->x[ip][1]);
	(*input->vars)["z"] = Var("z", s->x[ip][2]);
	if (xset) {
	  vx = xvalue.result(mpm);
	  s->v[ip][0] = vx;
	}
	if (yset) {
	  vy = yvalue.result(mpm);
	  s->v[ip][1] = vy;
	}
	if (zset) {
	  vz = zvalue.result(mpm);
	  s->v[ip][2] = vz;
	}
      }
    }
  }
}

/* ----------------------------------------------------------------------
 *
 *                    ***       Karamelo       ***
 *               Parallel Material Point Method Simulator
 * 
 * Copyright (2019) Alban de Vaucorbeil, alban.devaucorbeil@monash.edu
 * Materials Science and Engineering, Monash University
 * Clayton VIC 3800, Australia

 * This software is distributed under the GNU General Public License.
 *
 * ----------------------------------------------------------------------- */

#include <iostream>
#include <vector>
#include <string>
#include <Eigen/Eigen>
#include "fix_strain_energy.h"
#include "input.h"
#include "group.h"
#include "domain.h"
#include "input.h"
#include "update.h"
#include "output.h"
#include "math_special.h"
#include "universe.h"
#include "error.h"
#include "solid.h"

using namespace std;
using namespace FixConst;
using namespace MathSpecial;
using namespace Eigen;

FixStrainEnergy::FixStrainEnergy(MPM *mpm, vector<string> args) : Fix(mpm, args)
{
  if (args.size() < 3) {
    error->all(FLERR,"Error: too few arguments for fix_strain_energy: requires at least 3 arguments. " + to_string(args.size()) + " received.\n");
  }

  if (group->pon[igroup].compare("particles") !=0 && group->pon[igroup].compare("all") !=0) {
    error->all(FLERR, "fix_strain_energy needs to be given a group of particles" + group->pon[igroup] + ", " + args[2] + " is a group of " + group->pon[igroup] + ".\n");
  }

  cout << "Creating new fix FixStrainEnergy with ID: " << args[0] << endl;
  id = args[0];
}

FixStrainEnergy::~FixStrainEnergy()
{
}

void FixStrainEnergy::init()
{
}

void FixStrainEnergy::setup()
{
}

void FixStrainEnergy::setmask() {
  mask = 0;
  mask |= FINAL_INTEGRATE;
}


void FixStrainEnergy::final_integrate() {
  if (update->ntimestep != output->next && update->ntimestep != update->nsteps) return;
  // cout << "In FixStrainEnergy::post_particles_to_grid()\n";

  // Go through all the nodes in the group and set b to the right value:
  double ux, uy, uz;
    
  int solid = group->solid[igroup];

  double Es, Es_reduced;
  Solid *s;

  Es = 0;
  Es_reduced = 0;

  if (solid == -1) {
    for (int isolid = 0; isolid < domain->solids.size(); isolid++) {
      s = domain->solids[isolid];

      for (int in = 0; in < s->np_local; in++) {
	if (s->mask[in] & groupbit) {
	  Es += 0.5*s->vol[in]*(s->sigma[in](0,0)*s->strain_el[in](0,0)
				+ s->sigma[in](0,1)*s->strain_el[in](0,1)
				+ s->sigma[in](0,2)*s->strain_el[in](0,2)
				+ s->sigma[in](1,0)*s->strain_el[in](1,0)
				+ s->sigma[in](1,1)*s->strain_el[in](1,1)
				+ s->sigma[in](1,2)*s->strain_el[in](1,2)
				+ s->sigma[in](2,0)*s->strain_el[in](2,0)
				+ s->sigma[in](2,1)*s->strain_el[in](2,1)
				+ s->sigma[in](2,2)*s->strain_el[in](2,2));
	}
      }
    }
  } else {
    s = domain->solids[solid];

    for (int in = 0; in < s->np_local; in++) {
      if (s->mask[in] & groupbit) {
	Es += 0.5*s->vol[in]*(s->sigma[in](0,0)*s->strain_el[in](0,0)
			      + s->sigma[in](0,1)*s->strain_el[in](0,1)
			      + s->sigma[in](0,2)*s->strain_el[in](0,2)
			      + s->sigma[in](1,0)*s->strain_el[in](1,0)
			      + s->sigma[in](1,1)*s->strain_el[in](1,1)
			      + s->sigma[in](1,2)*s->strain_el[in](1,2)
			      + s->sigma[in](2,0)*s->strain_el[in](2,0)
			      + s->sigma[in](2,1)*s->strain_el[in](2,1)
			      + s->sigma[in](2,2)*s->strain_el[in](2,2));
      }
    }
  }

  // Reduce Es:
  MPI_Allreduce(&Es,&Es_reduced,1,MPI_DOUBLE,MPI_SUM,universe->uworld);
  (*input->vars)[id+"_s"]=Var(id+"_s", Es_reduced);
}

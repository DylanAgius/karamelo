#include <iostream>
#include "output.h"
#include "dump_pyplot.h"
#include "update.h"
#include "domain.h"
#include "solid.h"
#include "mpmtype.h"
#include "mpm_math.h"
#include <matplotlibcpp.h>
#include <vector>

using namespace std;
using namespace MPM_Math;
namespace plt = matplotlibcpp;


DumpPyPlot::DumpPyPlot(MPM *mpm, vector<string> args) : Dump(mpm, args)
{
  cout << "In DumpPyPlot::DumpPyPlot()" << endl;
  if (domain->dimension != 1 && domain->dimension != 2) {
    cout << "Error: cannot use dump_pyplot with dimensions different than 1 or 2!\n";
    exit(1);
  }
}

DumpPyPlot::~DumpPyPlot()
{
}


void DumpPyPlot::write()
{
  // Open dump file:
  size_t pos_asterisk = filename.find('*');
  string fdump;

  if (pos_asterisk >= 0)
    {
      // Replace the asterisk by ntimestep:
      fdump = filename.substr(0, pos_asterisk)
	+ to_string(update->ntimestep);
      if (filename.size()-pos_asterisk-1 > 0)
	fdump += filename.substr(pos_asterisk+1, filename.size()-pos_asterisk-1);
    }
  else fdump = filename;

  // cout << "Filemame for dump: " << fdump << endl;

  // Set the size of output image to 1200x780 pixels
  plt::figure_size(1200, 780);

  // Check how many different grids we have:
  vector<class Grid *> grids; // We will store the different pointers to grids here.
  bigint total_np = 0;

  for (int isolid=0; isolid < domain->solids.size(); isolid++) {
    total_np += domain->solids[isolid]->np;
    if (grids.size()==0) {
      grids.push_back(domain->solids[isolid]->grid);
    } else {
      // If the grid pointer is not present into grids, add it, otherwise continue:
      if ( find(grids.begin(), grids.end(), domain->solids[isolid]->grid) == grids.end() )
	grids.push_back(domain->solids[isolid]->grid);
    }
  }
    
  // Now loop over the grids to find how many elements there are in total:
  bigint total_nn = 0;
  for (auto g: grids) {
    total_nn += g->nnodes;
  }

  std::vector<double> xg(total_nn), yg(total_nn);
  std::vector<double> xp(total_np), yp(total_np);

  bigint ID = 0;
  int igrid = 0;
  for (auto g: grids) {
    for (bigint i=0; i<g->nnodes;i++) {
      xg[ID] = g->x[i][0];
      yg[ID] = g->x[i][1];
      ID++;
    }
  }

  ID = 0;
  for (int isolid=0; isolid < domain->solids.size(); isolid++) {
    Solid *s = domain->solids[isolid];
    for (bigint i=0; i<s->np;i++) {
      xp[ID] = s->x[i][0];
      yp[ID] = s->x[i][1];
      ID++;
    }
  }


  // Plot grid nodes and particles:
  plt::plot(xg, yg, "+");
  plt::plot(xp, yp, "o");

  if (update->method_style.compare("tlcpdi") == 0
      || update->method_style.compare("ulcpdi") == 0) {

    for (int isolid=0; isolid < domain->solids.size(); isolid++) {
      Solid *s = domain->solids[isolid];

      Eigen::Vector3d corner;
      vector<double> xcorner(s->nc, 0.0);
      vector<double> ycorner(s->nc, 0.0);

      for (bigint ip=0; ip<s->np;ip++) {
	// Calculate the coordinates of the particle domain's corners:
	  if (domain->dimension == 1) {
	    // 1st corner:
	    corner = s->x[ip] - s->rp[ip];
	    xcorner[0] = corner[0];

	    // 2nd corner:
	    corner = s->x[ip] - s->rp[ip];
	    xcorner[1] = corner[0];
	  }

	  if (domain->dimension == 2) {
	    // 1st corner:
	    corner = s->x[ip] - s->rp[2*ip] - s->rp[2*ip+1];
	    xcorner[0] = corner[0];
	    ycorner[0] = corner[1];
	    // 2nd corner:
	    corner = s->x[ip] + s->rp[2*ip] - s->rp[2*ip+1];
	    xcorner[1] = corner[0];
	    ycorner[1] = corner[1];
	    // 3rd corner:
	    corner = s->x[ip] + s->rp[2*ip] + s->rp[2*ip+1];
	    xcorner[2] = corner[0];
	    ycorner[2] = corner[1];
	    // 4th corner:
	    corner = s->x[ip] - s->rp[2*ip] - s->rp[2*ip+1];
	    xcorner[3] = corner[0];
	    ycorner[3] = corner[1];
	  }

	plt::plot(xcorner, ycorner, "d-");
      }
    }
  }

  // Save the image (file format is determined by the extension)
  plt::save(fdump);
  plt::show();
}

#include <mpi.h>
#include "mpm.h"
#include "solid.h"
#include "memory.h"
#include "update.h"
#include "method.h"
#include "domain.h"
#include "universe.h"
#include <vector>
#include <string>
#include <Eigen/Eigen>
#include "mpm_math.h"
#include <math.h>
#include "input.h"
#include "var.h"
#include <omp.h>
#include "error.h"

#ifdef DEBUG
#include <matplotlibcpp.h>
namespace plt = matplotlibcpp;
#endif

using namespace std;
using namespace Eigen;
using namespace MPM_Math;

Solid::Solid(MPM *mpm, vector<string> args) :
  Pointers(mpm)
{
  // Check that a method is available:
  if (update->method == NULL) {
    error->all(FLERR, "Error: a method should be defined before creating a solid!\n");
  }
  if (args.size() < 3) {
    error->all(FLERR, "Error: solid command not enough arguments.\n");
  }

  cout << "Creating new solid with ID: " << args[0] << endl;

  method_style = update->method_style;
  id = args[0];

  np = 0;

  ptag = NULL;

  x = x0 = NULL;
  rp = rp0 = NULL;
  xpc = xpc0 = NULL;

  if (update->method->is_CPDI) {
    nc = pow(2, domain->dimension);
  } else nc = 0;

  v = v_update = NULL;

  a = NULL;

  sigma = strain_el = vol0PK1 = L = F = R = U = D = Finv = Fdot = Di = NULL;

  mb = f = NULL;

  J = NULL;

  vol = vol0 = NULL;
  rho = rho0 = NULL;
  mass = NULL;
  eff_plastic_strain = NULL;
  eff_plastic_strain_rate = NULL;
  damage = NULL;
  damage_init = NULL;
  mask = NULL;

  mat = NULL;

  if (update->method->is_TL) grid = new Grid(mpm);
  else grid = domain->grid;

  numneigh_pn = numneigh_np = NULL;

  neigh_pn = neigh_np = NULL;

  wf_pn = wf_np = NULL;
  wfd_pn = wfd_np = NULL;

  dtCFL = 1.0e22;
  vtot = 0;

  // Set material and cellsize:
  options(&args, args.begin()+3);

  // Create particles:
  populate(args);
}

Solid::~Solid()
{
  if (ptag!=NULL) delete ptag;
  if (x0!=NULL) delete x0;
  if (x!=NULL) delete x;
  if (rp0!=NULL) delete rp0;
  if (rp!=NULL) delete rp;
  if (xpc0!=NULL) delete xpc0;
  if (xpc!=NULL) delete xpc;
  if (v!=NULL) delete v;
  if (v_update!=NULL) delete v_update;
  if (a!=NULL) delete a;
  if (mb!=NULL) delete mb;
  if (f!=NULL) delete f;
  if (sigma!=NULL) delete sigma;
  if (strain_el!=NULL) delete strain_el;
  if (vol0PK1!=NULL) delete vol0PK1;
  if (L!=NULL) delete L;
  if (F!=NULL) delete F;
  if (R!=NULL) delete R;
  if (U!=NULL) delete U;
  if (D!=NULL) delete D;
  if (Finv!=NULL) delete Finv;
  if (Fdot!=NULL) delete Fdot;
  if (Di!=NULL) delete Di;

  memory->destroy(J);
  memory->destroy(vol);
  memory->destroy(vol0);
  memory->destroy(rho);
  memory->destroy(rho0);
  memory->destroy(mass);
  memory->destroy(eff_plastic_strain);
  memory->destroy(eff_plastic_strain_rate);
  memory->destroy(damage);
  memory->destroy(damage_init);
  memory->destroy(mask);

  if (method_style.compare("tlmpm") == 0) delete grid;

  delete [] numneigh_pn;
  delete [] numneigh_np;

  delete [] neigh_pn;
  delete [] neigh_np;

  delete [] wf_pn;
  delete [] wf_np;

  delete [] wfd_pn;
  delete [] wfd_np;
}


void Solid::init()
{
  cout << "Bounds for " << id << ":\n";
  cout << "xlo xhi: " << solidlo[0] << " " << solidhi[0] << endl;
  cout << "ylo yhi: " << solidlo[1] << " " << solidhi[1] << endl;
  cout << "zlo zhi: " << solidlo[2] << " " << solidhi[2] << endl;

  // Calculate total volume:
  vtot = 0;
  for (int ip=0; ip<np; ip++){
    vtot += vol[ip];
  }

  cout << "Solid " << id << " total volume = " << vtot << endl;

  if (grid->nnodes == 0) grid->init(solidlo, solidhi);

  if (np == 0) {
    error->all(FLERR,"Error: solid does not have any particles.\n");
  } else {
      bigint nnodes = grid->nnodes;

      numneigh_pn = new int[np]();
      neigh_pn = new vector<int>[np];
      wf_pn = new vector<double>[np];
      wfd_pn = new vector< Vector3d >[np];

      if (nnodes) {
	numneigh_np = new int[nnodes]();
	neigh_np = new vector<int>[nnodes];
	wf_np = new vector<double>[nnodes];
	wfd_np = new vector< Vector3d >[nnodes];
      }
  }
}

void Solid::options(vector<string> *args, vector<string>::iterator it)
{
  cout << "In solid::options()" << endl;
  if (args->end() < it+2) {
    error->all(FLERR, "Error: not enough arguments.\n");
  }
  if (args->end() > it) {
    int iMat = material->find_material(*it);

    if (iMat == -1){
      cout << "Error: could not find material named " << *it << endl;
      error->all(FLERR,"\n");
    }

    mat = &material->materials[iMat]; // point mat to the right material

    it++;

    if (grid->cellsize == 0) grid->setup(*it); // set the grid cellsize

    it++;

    if (it != args->end()) {
      error->all(FLERR,"Error: too many arguments.\n");
    }
  }
}


void Solid::grow(int nparticles){
  np = nparticles;

  string str;

  str = "solid-" + id + ":ptag";
  cout << "Growing " << str << endl;
  if (ptag == NULL) ptag = new tagint[np];

  str = "solid-" + id + ":x0";
  cout << "Growing " << str << endl;
  if (x0 == NULL) x0 = new Eigen::Vector3d[np];

  str = "solid-" + id + ":x";
  cout << "Growing " << str << endl;
  if (x == NULL) x = new Eigen::Vector3d[np];

  if (method_style.compare("tlcpdi") == 0
      || method_style.compare("ulcpdi") == 0) {

    str = "solid-" + id + ":rp0";
    cout << "Growing " << str << endl;
    if (rp0 == NULL) rp0 = new Eigen::Vector3d[domain->dimension*np];

    str = "solid-" + id + ":rp";
    cout << "Growing " << str << endl;
    if (rp == NULL) rp = new Eigen::Vector3d[domain->dimension*np];
  }

  if (method_style.compare("tlcpdi2") == 0
      || method_style.compare("ulcpdi2") == 0) {

    str = "solid-" + id + ":xpc0";
    cout << "Growing " << str << endl;
    if (xpc0 == NULL) xpc0 = new Eigen::Vector3d[nc*np];

    str = "solid-" + id + ":xpc";
    cout << "Growing " << str << endl;
    if (xpc == NULL) xpc = new Eigen::Vector3d[nc*np];
  }

  str = "solid-" + id + ":v";
  cout << "Growing " << str << endl;
  if (v == NULL) v = new Eigen::Vector3d[np];

  str = "solid-" + id + ":v_update";
  cout << "Growing " << str << endl;
  if (v_update == NULL) v_update = new Eigen::Vector3d[np];

  str = "solid-" + id + ":a";
  cout << "Growing " << str << endl;
  if (a == NULL) a = new Eigen::Vector3d[np];

  str = "solid-" + id + ":mb";
  cout << "Growing " << str << endl;
  if (mb == NULL) mb = new Eigen::Vector3d[np];

  str = "solid-" + id + ":f";
  cout << "Growing " << str << endl;
  if (f == NULL) f = new Eigen::Vector3d[np];

  if (sigma == NULL) sigma = new Eigen::Matrix3d[np];

  if (strain_el == NULL) strain_el = new Eigen::Matrix3d[np];

  if (vol0PK1 == NULL) vol0PK1 = new Eigen::Matrix3d[np];

  if (L == NULL) L = new Eigen::Matrix3d[np];

  if (F == NULL) F = new Eigen::Matrix3d[np];

  if (R == NULL) R = new Eigen::Matrix3d[np];

  if (U == NULL) U = new Eigen::Matrix3d[np];

  if (D == NULL) D = new Eigen::Matrix3d[np];

  if (Finv == NULL) Finv = new Eigen::Matrix3d[np];

  if (Fdot == NULL) Fdot = new Eigen::Matrix3d[np];

  if (Di == NULL) Di = new Eigen::Matrix3d[np];


  str = "solid-" + id + ":vol0";
  cout << "Growing " << str << endl;
  vol0 = memory->grow(vol0, np, str);

  str = "solid-" + id + ":vol";
  cout << "Growing " << str << endl;
  vol = memory->grow(vol, np, str);

  str = "solid-" + id + ":rho0";
  cout << "Growing " << str << endl;
  rho0 = memory->grow(rho0, np, str);

  str = "solid-" + id + ":rho";
  cout << "Growing " << str << endl;
  rho = memory->grow(rho, np, str);

  str = "solid-" + id + ":mass";
  cout << "Growing " << str << endl;
  mass = memory->grow(mass, np, str);

  str = "solid-" + id + ":eff_plastic_strain";
  cout << "Growing " << str << endl;
  eff_plastic_strain = memory->grow(eff_plastic_strain, np, str);

  str = "solid-" + id + ":eff_plastic_strain_rate";
  cout << "Growing " << str << endl;
  eff_plastic_strain_rate = memory->grow(eff_plastic_strain_rate, np, str);

  str = "solid-" + id + ":damage";
  cout << "Growing " << str << endl;
  damage = memory->grow(damage, np, str);

  str = "solid-" + id + ":damage_init";
  cout << "Growing " << str << endl;
  damage_init = memory->grow(damage_init, np, str);

  str = "solid-" + id + ":mask";
  cout << "Growing " << str << endl;
  mask = memory->grow(mask, np, str);

  for (int i=0; i<np; i++) mask[i] = 1;

  str = "solid-" + id + ":J";
  cout << "Growing " << str << endl;
  J = memory->grow(J, np, str);
}

void Solid::compute_mass_nodes(bool reset)
{
  int ip;

  for (int in=0; in<grid->nnodes; in++){
    if (reset) grid->mass[in] = 0;

    for (int j=0; j<numneigh_np[in];j++){
      ip = neigh_np[in][j];
      grid->mass[in] += wf_np[in][j] * mass[ip];
    }
  }
  return;
}

void Solid::compute_velocity_nodes(bool reset)
{
  Eigen::Vector3d *vn = grid->v;
  Eigen::Vector3d vtemp;
  double *massn = grid->mass;
  int ip;

  for (int in=0; in<grid->nnodes; in++) {
    if (reset) vn[in].setZero();
    if (massn[in] > 0) {
      vtemp.setZero();
      for (int j=0; j<numneigh_np[in];j++){
	ip = neigh_np[in][j];
	vtemp += (wf_np[in][j] * mass[ip]) * v[ip];
	//vn[in] += (wf_np[in][j] * mass[ip]) * v[ip]/ massn[in];
      }
      vtemp /= massn[in];
      vn[in] += vtemp;
    }
  }
}

void Solid::compute_velocity_nodes_APIC(bool reset)
{
  Eigen::Vector3d *x0n = grid->x0;
  Eigen::Vector3d *vn = grid->v;
  double *massn = grid->mass;
  int ip;

  for (int in=0; in<grid->nnodes; in++) {
    if (reset) vn[in].setZero();
    if (massn[in] > 0) {
      for (int j=0; j<numneigh_np[in];j++){
	ip = neigh_np[in][j];
	vn[in] += (wf_np[in][j] * mass[ip]) * (v[ip] + Fdot[ip]*(x0n[in] - x0[ip]))/ massn[in];
      }
    }
  }
}

void Solid::compute_external_forces_nodes(bool reset)
{
  Eigen::Vector3d *mbn = grid->mb;
  double *massn = grid->mass;
  int ip;

  for (int in=0; in<grid->nnodes; in++) {
    if (reset) mbn[in].setZero();
    if (massn[in] > 0) {
      for (int j=0; j<numneigh_np[in];j++){
	ip = neigh_np[in][j];
	mbn[in] += wf_np[in][j] * mb[ip];
      }
    }
  }
}

void Solid::compute_internal_forces_nodes_TL()
{
  Eigen::Vector3d *fn = grid->f;
  Eigen::Vector3d ftemp;
  int ip;

  for (int in=0; in<grid->nnodes; in++) {
    //fn[in].setZero();
    ftemp.setZero();
    for (int j=0; j<numneigh_np[in];j++){
      ip = neigh_np[in][j];
      ftemp -= vol0PK1[ip] * wfd_np[in][j];
      //fn[in] -= (vol0PK1[ip] * wfd_np[in][j]);
    }
    fn[in] = ftemp;
  }
}

void Solid::compute_internal_forces_nodes_UL(bool reset)
{
  Eigen::Vector3d *fn = grid->f;
  double *massn = grid->mass;
  int ip;

  for (int in=0; in<grid->nnodes; in++) {
    if (reset) fn[in].setZero();
    for (int j=0; j<numneigh_np[in];j++){
      ip = neigh_np[in][j];
      fn[in] -= vol[ip] * (sigma[ip] * wfd_np[in][j]);
    }
  }
}


void Solid::compute_particle_velocities()
{
  Eigen::Vector3d *vn_update = grid->v_update;
  int in;

  for (int ip=0; ip<np; ip++){
    v_update[ip].setZero();
    for (int j=0; j<numneigh_pn[ip]; j++){
      in = neigh_pn[ip][j];
      v_update[ip] += wf_pn[ip][j] * vn_update[in];
    }
  }
}

void Solid::compute_particle_acceleration()
{
  double inv_dt = 1.0/update->dt;
  
  Eigen::Vector3d *vn_update = grid->v_update;
  Eigen::Vector3d *vn = grid->v;

  int in;

  for (int ip=0; ip<np; ip++){
    a[ip].setZero();
    for (int j=0; j<numneigh_pn[ip]; j++){
      in = neigh_pn[ip][j];
      a[ip] += wf_pn[ip][j] * (vn_update[in] - vn[in]);
    }
    a[ip] *= inv_dt;
    f[ip] = a[ip] / mass[ip];
  }
}

void Solid::update_particle_position()
{
  bool ul;

  if (update->method_style.compare("tlmpm") != 0) ul = true;
  else ul = false;

  for (int ip=0; ip<np; ip++) {
    x[ip] += update->dt*v_update[ip];
    if (ul) {
      // Check if the particle is within the box's domain:
      if (domain->inside(x[ip]) == 0) {
	error->all(FLERR,"Error: Particle " + to_string(ip) + " left the domain (" +
		   to_string(domain->boxlo[0]) + ","+ to_string(domain->boxhi[0]) + "," +
		   to_string(domain->boxlo[1]) + ","+ to_string(domain->boxhi[1]) + "," +
		   to_string(domain->boxlo[2]) + ","+ to_string(domain->boxhi[2]) + ",):\n");
      }
    }
  }
}

void Solid::update_particle_velocities(double FLIP)
{
  for (int ip=0; ip<np; ip++) {
    v[ip] = (1 - FLIP) * v_update[ip] + FLIP*(v[ip] + update->dt*a[ip]);
  }
}

void Solid::compute_rate_deformation_gradient_TL()
{
  int in;
  Eigen::Vector3d *vn = grid->v;

  if (domain->dimension == 1) {
    for (int ip=0; ip<np; ip++){
      Fdot[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	Fdot[ip](0,0) += vn[in][0]*wfd_pn[ip][j][0];
      }
    }
  } else if (domain->dimension == 2) {
    for (int ip=0; ip<np; ip++){
      Fdot[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	Fdot[ip](0,0) += vn[in][0]*wfd_pn[ip][j][0];
	Fdot[ip](0,1) += vn[in][0]*wfd_pn[ip][j][1];
	Fdot[ip](1,0) += vn[in][1]*wfd_pn[ip][j][0];
	Fdot[ip](1,1) += vn[in][1]*wfd_pn[ip][j][1];
      }
    }
  } else if (domain->dimension == 3) {
    for (int ip=0; ip<np; ip++){
      Fdot[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	Fdot[ip](0,0) += vn[in][0]*wfd_pn[ip][j][0];
	Fdot[ip](0,1) += vn[in][0]*wfd_pn[ip][j][1];
	Fdot[ip](0,2) += vn[in][0]*wfd_pn[ip][j][2];
	Fdot[ip](1,0) += vn[in][1]*wfd_pn[ip][j][0];
	Fdot[ip](1,1) += vn[in][1]*wfd_pn[ip][j][1];
	Fdot[ip](1,2) += vn[in][1]*wfd_pn[ip][j][2];
	Fdot[ip](2,0) += vn[in][2]*wfd_pn[ip][j][0];
	Fdot[ip](2,1) += vn[in][2]*wfd_pn[ip][j][1];
	Fdot[ip](2,2) += vn[in][2]*wfd_pn[ip][j][2];
      }
    }
  }
}

void Solid::compute_rate_deformation_gradient_UL_MUSL()
{
  int in;
  Eigen::Vector3d *vn = grid->v;

  if (domain->dimension == 1) {
    for (int ip=0; ip<np; ip++){
      L[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	L[ip](0,0) += vn[in][0]*wfd_pn[ip][j][0];
      }
    }
  } else if (domain->dimension == 2) {
    for (int ip=0; ip<np; ip++){
      L[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	L[ip](0,0) += vn[in][0]*wfd_pn[ip][j][0];
	L[ip](0,1) += vn[in][0]*wfd_pn[ip][j][1];
	L[ip](1,0) += vn[in][1]*wfd_pn[ip][j][0];
	L[ip](1,1) += vn[in][1]*wfd_pn[ip][j][1];
      }
    }
  } else if (domain->dimension == 3) {
    for (int ip=0; ip<np; ip++){
      L[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	L[ip](0,0) += vn[in][0]*wfd_pn[ip][j][0];
	L[ip](0,1) += vn[in][0]*wfd_pn[ip][j][1];
	L[ip](0,2) += vn[in][0]*wfd_pn[ip][j][2];
	L[ip](1,0) += vn[in][1]*wfd_pn[ip][j][0];
	L[ip](1,1) += vn[in][1]*wfd_pn[ip][j][1];
	L[ip](1,2) += vn[in][1]*wfd_pn[ip][j][2];
	L[ip](2,0) += vn[in][2]*wfd_pn[ip][j][0];
	L[ip](2,1) += vn[in][2]*wfd_pn[ip][j][1];
	L[ip](2,2) += vn[in][2]*wfd_pn[ip][j][2];
      }
    }
  }
}

void Solid::compute_rate_deformation_gradient_UL_USL()
{
  int in;
  Eigen::Vector3d *vn = grid->v_update;

  if (domain->dimension == 1) {
    for (int ip=0; ip<np; ip++){
      L[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	L[ip](0,0) += vn[in][0]*wfd_pn[ip][j][0];
      }
    }
  } else if (domain->dimension == 2) {
    for (int ip=0; ip<np; ip++){
      L[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	L[ip](0,0) += vn[in][0]*wfd_pn[ip][j][0];
	L[ip](0,1) += vn[in][0]*wfd_pn[ip][j][1];
	L[ip](1,0) += vn[in][1]*wfd_pn[ip][j][0];
	L[ip](1,1) += vn[in][1]*wfd_pn[ip][j][1];
      }
    }
  } else if (domain->dimension == 3) {
    for (int ip=0; ip<np; ip++){
      L[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	L[ip](0,0) += vn[in][0]*wfd_pn[ip][j][0];
	L[ip](0,1) += vn[in][0]*wfd_pn[ip][j][1];
	L[ip](0,2) += vn[in][0]*wfd_pn[ip][j][2];
	L[ip](1,0) += vn[in][1]*wfd_pn[ip][j][0];
	L[ip](1,1) += vn[in][1]*wfd_pn[ip][j][1];
	L[ip](1,2) += vn[in][1]*wfd_pn[ip][j][2];
	L[ip](2,0) += vn[in][2]*wfd_pn[ip][j][0];
	L[ip](2,1) += vn[in][2]*wfd_pn[ip][j][1];
	L[ip](2,2) += vn[in][2]*wfd_pn[ip][j][2];
      }
    }
  }
}

void Solid::compute_deformation_gradient()
{
  int in;
  Eigen::Vector3d *xn = grid->x;
  Eigen::Vector3d *x0n = grid->x0;
  Eigen::Vector3d dx;
  Eigen::Matrix3d Ftemp, eye;
  eye.setIdentity();

  if (domain->dimension == 1) {
    for (int ip=0; ip<np; ip++){
      // F[ip].setZero();
      Ftemp.setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	dx = xn[in] - x0n[in];
	Ftemp(0,0) += dx[0]*wfd_pn[ip][j][0];
      }
      F[ip](0,0) = Ftemp(0,0) +1;
    }
  } else if (domain->dimension == 2) {
    for (int ip=0; ip<np; ip++){
      // F[ip].setZero();
      Ftemp.setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	dx = xn[in] - x0n[in];
	Ftemp(0,0) += dx[0]*wfd_pn[ip][j][0];
	Ftemp(0,1) += dx[0]*wfd_pn[ip][j][1];
	Ftemp(1,0) += dx[1]*wfd_pn[ip][j][0];
	Ftemp(1,1) += dx[1]*wfd_pn[ip][j][1];
      }
      F[ip] = Ftemp + eye;
    }
  } else if (domain->dimension == 3) {
    for (int ip=0; ip<np; ip++){
      // F[ip].setZero();
      Ftemp.setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	dx = xn[in] - x0n[in];
	Ftemp(0,0) += dx[0]*wfd_pn[ip][j][0];
	Ftemp(0,1) += dx[0]*wfd_pn[ip][j][1];
	Ftemp(0,2) += dx[0]*wfd_pn[ip][j][2];
	Ftemp(1,0) += dx[1]*wfd_pn[ip][j][0];
	Ftemp(1,1) += dx[1]*wfd_pn[ip][j][1];
	Ftemp(1,2) += dx[1]*wfd_pn[ip][j][2];
	Ftemp(2,0) += dx[2]*wfd_pn[ip][j][0];
	Ftemp(2,1) += dx[2]*wfd_pn[ip][j][1];
	Ftemp(2,2) += dx[2]*wfd_pn[ip][j][2];
      }
      //F[ip].noalias() += eye;
      F[ip] = Ftemp + eye;

    }
  }
}


void Solid::compute_rate_deformation_gradient_TL_APIC()
{
  int in;
  Eigen::Vector3d *x0n = grid->x0;
  //Eigen::Vector3d *vn = grid->v;
  Eigen::Vector3d *vn = grid->v_update;
  Eigen::Vector3d dx;

  if (domain->dimension == 1) {
    for (int ip=0; ip<np; ip++){
      Fdot[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	dx = x0n[in] - x0[ip];
	Fdot[ip](0,0) += vn[in][0]*dx[0]*wf_pn[ip][j];
      }
      Fdot[ip] *= Di[ip];
    }
  } else if (domain->dimension == 2) {
    for (int ip=0; ip<np; ip++){
      Fdot[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	dx = x0n[in] - x0[ip];
	Fdot[ip](0,0) += vn[in][0]*dx[0]*wf_pn[ip][j];
	Fdot[ip](0,1) += vn[in][0]*dx[1]*wf_pn[ip][j];
	Fdot[ip](1,0) += vn[in][1]*dx[0]*wf_pn[ip][j];
	Fdot[ip](1,1) += vn[in][1]*dx[1]*wf_pn[ip][j];
      }
      Fdot[ip] *= Di[ip];
    }
  } else if (domain->dimension == 3) {
    for (int ip=0; ip<np; ip++){
      Fdot[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	dx = x0n[in] - x0[ip];
	Fdot[ip](0,0) += vn[in][0]*dx[0]*wf_pn[ip][j];
	Fdot[ip](0,1) += vn[in][0]*dx[1]*wf_pn[ip][j];
	Fdot[ip](0,2) += vn[in][0]*dx[2]*wf_pn[ip][j];
	Fdot[ip](1,0) += vn[in][1]*dx[0]*wf_pn[ip][j];
	Fdot[ip](1,1) += vn[in][1]*dx[1]*wf_pn[ip][j];
	Fdot[ip](1,2) += vn[in][1]*dx[2]*wf_pn[ip][j];
	Fdot[ip](2,0) += vn[in][2]*dx[0]*wf_pn[ip][j];
	Fdot[ip](2,1) += vn[in][2]*dx[1]*wf_pn[ip][j];
	Fdot[ip](2,2) += vn[in][2]*dx[2]*wf_pn[ip][j];
      }
      Fdot[ip] *= Di[ip];
    }
  }
}

void Solid::compute_rate_deformation_gradient_UL_APIC()
{
  int in;
  Eigen::Vector3d *x0n = grid->x0;
  //Eigen::Vector3d *vn = grid->v;
  Eigen::Vector3d *vn = grid->v_update;
  Eigen::Vector3d dx;

  if (domain->dimension == 1) {
    for (int ip=0; ip<np; ip++){
      L[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	dx = x0n[in] - x0[ip];
	L[ip](0,0) += vn[in][0]*dx[0]*wf_pn[ip][j];
      }
      L[ip] *= Di[ip];
    }
  } else if (domain->dimension == 2) {
    for (int ip=0; ip<np; ip++){
      L[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	dx = x0n[in] - x0[ip];
	L[ip](0,0) += vn[in][0]*dx[0]*wf_pn[ip][j];
	L[ip](0,1) += vn[in][0]*dx[1]*wf_pn[ip][j];
	L[ip](1,0) += vn[in][1]*dx[0]*wf_pn[ip][j];
	L[ip](1,1) += vn[in][1]*dx[1]*wf_pn[ip][j];
      }
      L[ip] *= Di[ip];
    }
  } else if (domain->dimension == 3) {
    for (int ip=0; ip<np; ip++){
      L[ip].setZero();
      for (int j=0; j<numneigh_pn[ip]; j++){
	in = neigh_pn[ip][j];
	dx = x0n[in] - x0[ip];
	L[ip](0,0) += vn[in][0]*dx[0]*wf_pn[ip][j];
	L[ip](0,1) += vn[in][0]*dx[1]*wf_pn[ip][j];
	L[ip](0,2) += vn[in][0]*dx[2]*wf_pn[ip][j];
	L[ip](1,0) += vn[in][1]*dx[0]*wf_pn[ip][j];
	L[ip](1,1) += vn[in][1]*dx[1]*wf_pn[ip][j];
	L[ip](1,2) += vn[in][1]*dx[2]*wf_pn[ip][j];
	L[ip](2,0) += vn[in][2]*dx[0]*wf_pn[ip][j];
	L[ip](2,1) += vn[in][2]*dx[1]*wf_pn[ip][j];
	L[ip](2,2) += vn[in][2]*dx[2]*wf_pn[ip][j];
      }
      L[ip] *= Di[ip];
    }
  }
}

void Solid::update_deformation_gradient()
{
  bool status, tl, nh;
  Eigen::Matrix3d U;
  Eigen::Matrix3d eye;
  eye.setIdentity();

  if (update->method_style.compare("tlmpm") == 0) tl = true;
  else tl = false;

  if ((mat->eos!=NULL) && (mat->strength!=NULL)) nh = true;
  else nh = false;

  for (int ip=0; ip<np; ip++){
    
    if (tl) F[ip] += update->dt * Fdot[ip];
    else F[ip] = (eye+update->dt*L[ip]) * F[ip];

    Finv[ip] = F[ip].inverse();
    J[ip] = F[ip].determinant();

    if (J[ip] < 0.0) {
      cout << "Error: J[" << ip << "]<0.0 == " << J[ip] << endl;
      cout << "F[" << ip << "]:" << endl << F[ip] << endl;
      error->all(FLERR,"");
    }

    vol[ip] = J[ip] * vol0[ip];
    rho[ip] = rho0[ip] / J[ip];

    if (nh) {
      // Only done if not Neo-Hookean:
      if (update->method_style.compare("tlmpm") == 0)
	L[ip] = Fdot[ip] * Finv[ip];
      // else
      //   Fdot[ip] = L[ip]*F[ip];

      status = PolDec(F[ip], R[ip], U, false); // polar decomposition of the deformation gradient, F = R * U
      if (update->method_style.compare("tlmpm") == 0)
	D[ip] = 0.5 * (R[ip].transpose() * (L[ip] + L[ip].transpose()) * R[ip]);
      else D[ip] = 0.5 * (L[ip] + L[ip].transpose());

      if (!status) {
	cout << "Polar decomposition of deformation gradient failed for particle " << ip << ".\n";
	cout << "F:" << endl << F[ip] << endl;
	cout << "timestep" << endl << update->ntimestep << endl;
	error->all(FLERR,"");
      }
    }

    // strain_increment[ip] = update->dt * D[ip];
  }
}

void Solid::update_stress()
{
  min_inv_p_wave_speed = 1.0e22;
  double pH, plastic_strain_increment;
  Matrix3d eye, sigma_dev, FinvT, PK1;
  bool tl, nh;

  if ((mat->eos!=NULL) && (mat->strength!=NULL)) nh = false;
  else nh = true;

  if (update->method_style.compare("tlmpm") == 0) tl = true;
  else tl = false;

  eye.setIdentity();

  //# pragma omp parallel for
  for (int ip=0; ip<np; ip++){
    if (nh) {

      // Neo-Hookean material:
      FinvT = Finv[ip].transpose();
      PK1 = mat->G*(F[ip] - FinvT) + mat->lambda*log(J[ip])*FinvT;
      vol0PK1[ip] = vol0[ip]*PK1;
      sigma[ip] = 1.0/J[ip]*(F[ip]*PK1.transpose());
      strain_el[ip] = 0.5*(F[ip].transpose()*F[ip] - eye);//update->dt * D[ip];

    } else {

      pH = mat->eos->compute_pressure(J[ip], rho[ip], 0, damage[ip]);
      sigma_dev = mat->strength->update_deviatoric_stress(sigma[ip], D[ip], plastic_strain_increment, eff_plastic_strain[ip], eff_plastic_strain_rate[ip], damage[ip]);

      eff_plastic_strain[ip] += plastic_strain_increment;

      // // compute a characteristic time over which to average the plastic strain
      double tav = 1000 * grid->cellsize / mat->signal_velocity;
      eff_plastic_strain_rate[ip] -= eff_plastic_strain_rate[ip] * update->dt / tav;
      eff_plastic_strain_rate[ip] += plastic_strain_increment / tav;
      eff_plastic_strain_rate[ip] = MAX(0.0, eff_plastic_strain_rate[ip]);

      if (mat->damage != NULL)
	mat->damage->compute_damage(damage_init[ip], damage[ip], pH, sigma_dev, eff_plastic_strain_rate[ip], plastic_strain_increment);
      sigma[ip] = -pH*eye + sigma_dev;

      if (damage[ip] > 1e-10) {
	strain_el[ip] = (update->dt*D[ip].trace() + strain_el[ip].trace())/3.0*eye + sigma_dev/(mat->G*(1-damage[ip]));
      } else {
	strain_el[ip] =  (update->dt*D[ip].trace() + strain_el[ip].trace())/3.0*eye;
      }

      if (tl) {
	vol0PK1[ip] = vol0[ip]*J[ip] * (R[ip] * sigma[ip] * R[ip].transpose()) * Finv[ip].transpose();
      }
    }
  }

  double min_h_ratio = 1.0e22;
  double four_third = 1.333333333333333333333333333333333333333;
  for (int ip=0; ip<np; ip++){
    min_inv_p_wave_speed = MIN(min_inv_p_wave_speed, rho[ip] / (mat->K + four_third * mat->G));

    min_h_ratio = MIN(min_h_ratio, F[ip](0,0)*F[ip](0,0) + F[ip](0,1)*F[ip](0,1) + F[ip](0,2)*F[ip](0,2));
    min_h_ratio = MIN(min_h_ratio, F[ip](1,0)*F[ip](1,0) + F[ip](1,1)*F[ip](1,1) + F[ip](1,2)*F[ip](1,2));
    min_h_ratio = MIN(min_h_ratio, F[ip](2,0)*F[ip](2,0) + F[ip](2,1)*F[ip](2,1) + F[ip](2,2)*F[ip](2,2));

    if (std::isnan(min_inv_p_wave_speed)) {
      cout << "Error: min_inv_p_wave_speed is nan with ip=" << ip << ", rho[ip]=" << rho[ip] << ", K=" << mat->K << ", G=" << mat->G << endl;
      error->all(FLERR, "");
    } else if (min_inv_p_wave_speed < 0.0) {
      cout << "Error: min_inv_p_wave_speed= " << min_inv_p_wave_speed << " with ip=" << ip << ", rho[ip]=" << rho[ip] << ", K=" << mat->K << ", G=" << mat->G << endl;
      error->all(FLERR, "");
    }

  }
  min_inv_p_wave_speed = sqrt(min_inv_p_wave_speed);
  dtCFL = MIN(dtCFL, min_inv_p_wave_speed * grid->cellsize * sqrt(min_h_ratio));
  if (std::isnan(dtCFL)) {
      cout << "Error: dtCFL = " << dtCFL << "\n";
      cout << "min_inv_p_wave_speed = " << min_inv_p_wave_speed << ", grid->cellsize=" << grid->cellsize << endl;
      error->all(FLERR, "");
  }
}


void Solid::compute_inertia_tensor(string form_function) {

  // int in;
  // Eigen::Vector3d *x0n = grid->x0;
  // Eigen::Vector3d *vn_update = grid->v_update;
  // Eigen::Vector3d dx;

  // if (domain->dimension == 2) {
  //   for (int ip=0; ip<np; ip++){
  //     Di[ip].setZero();
  //     for (int j=0; j<numneigh_pn[ip]; j++){
  // 	in = neigh_pn[ip][j];
  // 	dx = x0n[in] - x0[ip];
  // 	Di[ip](0,0) += wf_pn[ip][j]*(dx[0]*dx[0]);
  // 	Di[ip](0,1) += wf_pn[ip][j]*(dx[0]*dx[1]);
  // 	Di[ip](1,1) += wf_pn[ip][j]*(dx[1]*dx[1]);
  //     }
  //     Di[ip](1,0) = Di[ip](0,1);
  //     cout << "Di[" << ip << "]=\n" << Di[ip] << endl;
  //   }
  // } else if (domain->dimension == 3) {
  //   for (int ip=0; ip<np; ip++){
  //     Di[ip].setZero();
  //     for (int j=0; j<numneigh_pn[ip]; j++){
  // 	in = neigh_pn[ip][j];
  // 	dx = x0n[in] - x0[ip];
  // 	Di[ip](0,0) += wf_pn[ip][j]*(dx[0]*dx[0]);
  // 	Di[ip](0,1) += wf_pn[ip][j]*(dx[0]*dx[1]);
  // 	Di[ip](0,2) += wf_pn[ip][j]*(dx[0]*dx[2]);
  // 	Di[ip](1,1) += wf_pn[ip][j]*(dx[1]*dx[1]);
  // 	Di[ip](1,2) += wf_pn[ip][j]*(dx[1]*dx[2]);
  // 	Di[ip](2,2) += wf_pn[ip][j]*(dx[2]*dx[2]);
  //     }
  //     Di[ip](1,0) = Di[ip](0,1);
  //     Di[ip](2,1) = Di[ip](1,2);
  //     Di[ip](2,0) = Di[ip](0,2);
  //     cout << "Di[" << ip << "]=\n" << Di[ip] << endl;
  //   }
  // }

  // Di[ip] = Di[ip].inverse();

  Eigen::Matrix3d eye;
  eye.setIdentity();

  double cellsizeSqInv = 1.0/(grid->cellsize*grid->cellsize);

  for (int ip=0; ip<np; ip++){
    if ( form_function.compare("linear") == 0) {
      // If the form function is linear:
      Di[ip] = 16.0 / 3.0 * cellsizeSqInv * eye;
    } else if ( form_function.compare("quadratic-spline") == 0) {
      // If the form function is a quadratic spline:
      Di[ip] = 4.0 * cellsizeSqInv * eye;
    } else if ( form_function.compare("cubic-spline") == 0) {
      // If the form function is a cubic spline:
      Di[ip] = 3.0 * cellsizeSqInv * eye;
    } else if ( form_function.compare("Bernstein-quadratic") == 0)
      Di[ip] = 12.0 * cellsizeSqInv * eye;
    //cout << "Di[" << ip << "]=\n" << Di[ip] << endl;
  }
}

void Solid::copy_particle(int i, int j) {
  x0[j] = x0[i];
  x[j] = x[i];
  v[j] = v[i];
  v_update[j] = v[i];
  a[j] = a[i];
  mb[j] = mb[i];
  f[j] = f[i];
  vol0[j] = vol0[i];
  vol[j]= vol[i];
  rho0[j] = rho0[i];
  rho[j] = rho[i];
  mass[j] = mass[i];
  eff_plastic_strain[j] = eff_plastic_strain[i];
  eff_plastic_strain_rate[j] = eff_plastic_strain_rate[i];
  damage[j] = damage[i];
  damage_init[j] = damage_init[i];
  sigma[j] = sigma[i];
  vol0PK1[j] = vol0PK1[i];
  L[j] = L[i];
  F[j] = F[i];
  R[j] = R[i];
  U[j] = U[i];
  D[j] = D[i];
  Finv[j] = Finv[i];
  Fdot[j] = Fdot[i];
  J[j] = J[i];
}

void Solid::populate(vector<string> args) {

  cout << "Solid delimitated by region ID: " << args[1] << endl;

  // Look for region ID:
  int iregion = domain->find_region(args[1]);
  if (iregion == -1) {
    error->all(FLERR, "Error: region ID " + args[1] + " not does not exist.\n");
  }

  double *sublo = domain->sublo;
  double *subhi = domain->subhi;

  vector<double> limits = domain->regions[iregion]->limits();

  solidlo[0] = limits[0];
  solidhi[0] = limits[1];
  solidlo[1] = limits[2];
  solidhi[1] = limits[3];
  solidlo[2] = limits[4];
  solidhi[2] = limits[5];

  solidsublo[0] = MAX(solidlo[0], sublo[0]);
  solidsublo[1] = MAX(solidlo[1], sublo[1]);
  solidsublo[2] = MAX(solidlo[2], sublo[2]);

  solidsubhi[0] = MIN(solidhi[0], subhi[0]);
  solidsubhi[1] = MIN(solidhi[1], subhi[1]);
  solidsubhi[2] = MIN(solidhi[2], subhi[2]);

#ifdef DEBUG
  cout << "proc " << universe->me << "\tsolidsublo=[" << solidsublo[0] << "," << solidsublo[1] << "," << solidsublo[2] << "]\t solidsubhi=["<< solidsubhi[0] << "," << solidsubhi[1] << "," << solidsubhi[2] << "]\n";
#endif

  // Calculate total number of particles np:
  int nx, ny, nz, nsubx, nsuby, nsubz;
  double delta;
  double hdelta;
  double Lx, Ly, Lz, Lsubx, Lsuby, Lsubz;

  delta = grid->cellsize;
  
  if (grid->nnodes == 0) {
    // The grid will be ajusted to the solid's domain (good for TLMPM).

    // and we need to create the corresponding grid:
    grid->init(solidlo, solidhi);
  }

  Lx = solidhi[0]-solidlo[0];
  if (domain->dimension >= 2) Ly = solidhi[1]-solidlo[1];
  if (domain->dimension == 3) Lz = solidhi[2]-solidlo[2];

  Lsubx = solidsubhi[0]-solidsublo[0];
  if (domain->dimension >= 2) Lsuby = solidsubhi[1]-solidsublo[1];
  if (domain->dimension == 3) Lsubz = solidsubhi[2]-solidsublo[2];


  nx = (int) (Lx/delta);
  while (nx*delta <= Lx-0.5*delta) nx++;

  nsubx = (int) (Lsubx/delta);
  while (nsubx*delta <= Lsubx-0.5*delta) nsubx++;
  nsubx++;

  if (domain->dimension >= 2) {
    ny = (int) (Ly/delta);
    while (ny*delta <= Ly-0.5*delta) ny++;

    nsuby = (int) (Lsuby/delta);
    while (nsuby*delta <= Lsuby-0.5*delta) nsuby++;
    nsuby++;
  } else {
    ny = 1;
    nsuby = 1;
  }

  if (domain->dimension == 3) {
    nz = (int) (Lz/delta);
    while (nz*delta <= Lz-0.5*delta) nz++;

    nsubz = (int) (Lsubz/delta);
    while (nsubz*delta <= Lsubz-0.5*delta) nsubz++;
    nsubz++;
  } else {
    nz = 1;
    nsubz = 1;
  }

#ifdef DEBUG
  cout << "proc " << universe->me << "\tLsub=[" << Lsubx << "," << Lsuby << "," << Lsubz << "]\t nsub=["<< nsubx << "," << nsuby << "," << nsubz << "]\n";
#endif

  np_local = nsubx*nsuby*nsubz;

  // Create particles:


  cout << "delta = " << delta << endl;

  int l=0;
  double vol_;

  if (domain->dimension == 1) vol_ = delta;
  else if (domain->dimension == 2) vol_ = delta*delta;
  else vol_ = delta*delta*delta;

  double mass_ = mat->rho0 * vol_;

  int np_per_cell = (int) input->parsev(args[2]);

  double *boundlo;
  if (update->method->is_TL)
    boundlo = solidlo;
  else boundlo = domain->boxlo;

  double xi = 0.5;
  double lp = delta;
  int nip = 1;
  vector<double> intpoints;

  if (np_per_cell == 1) {
    // One particle per cell at the center:

    xi = 0.5;
    lp *= 0.5;
    nip = 1;

    intpoints = {0, 0, 0};

  } else if (np_per_cell == 2) {
    // Quadratic elements:

    if (domain->dimension == 1) nip = 2;
    else if (domain->dimension == 2) nip = 4;
    else nip = 8;

    if (nc==0) xi= 0.5/sqrt(3.0);
    else xi = 0.25;

    lp *= 0.25;

    intpoints = {-xi, -xi, -xi,
		 -xi, xi, -xi,
		 xi, -xi, -xi,
		 xi, xi, -xi,
		 -xi, -xi, xi,
		 -xi, xi, xi,
		 xi, -xi, xi,
		 xi, xi, xi};


  } else if (np_per_cell == 3) {
    // Berstein elements:

    if (nc == 0) xi = 0.7746/2;
    else xi = 1.0/3.0;

    lp *= 1.0/6.0;
    nip = 27;
    if (domain->dimension == 1) nip = 3;
    else if (domain->dimension == 2) nip = 9;
    else nip = 27;

    intpoints = {-xi, -xi, -xi,
		 -xi, 0, -xi,
		 -xi, xi, -xi,
		 0, -xi, -xi,
		 0, 0, -xi,
		 0, xi, -xi,
		 xi, -xi, -xi,
		 xi, 0, -xi,
		 xi, xi, -xi,
		 -xi, -xi, 0,
		 -xi, 0, 0,
		 -xi, xi, 0,
		 0, -xi, 0,
		 0, 0, 0,
		 0, xi, 0,
		 xi, -xi, 0,
		 xi, 0, 0,
		 xi, xi, 0,
		 -xi, -xi, xi,
		 -xi, 0, xi,
		 -xi, xi, xi,
		 0, -xi, xi,
		 0, 0, xi,
		 0, xi, xi,
		 xi, -xi, xi,
		 xi, 0, xi,
		 xi, xi, xi};

  } else {
    error->all(FLERR, "Error: solid command 4th argument should be 1 or 2, but " + to_string((int) input->parsev(args[3])) + "received.\n");
  }

  np_local *= nip;
#ifdef DEBUG
  cout << "proc " << universe->me << "\tnp_local=" << np_local << endl;
#endif
  mass_ /= (double) nip;
  vol_ /= (double) nip;

  // Allocate the space in the vectors for np particles:
  grow(np_local);

  int dim = domain->dimension;
  //checkIfInRegion = false;

  int nsubx0, nsuby0, nsubz0;
  nsubx0 = (int) (sublo[0] - boundlo[0])/delta;
  nsuby0 = (int) (sublo[1] - boundlo[1])/delta;
  nsubz0 = (int) (sublo[2] - boundlo[2])/delta;

#ifdef DEBUG
  std::vector<double> x2plot, y2plot;
#endif

  double Loffset[3] = {MAX(0.0 ,domain->sublo[0] - boundlo[0]),
		       MAX(0.0 ,domain->sublo[1] - boundlo[1]),
		       MAX(0.0 ,domain->sublo[2] - boundlo[2])};

  int noffset[3] = {(int) (Loffset[0]/delta),
		    (int) (Loffset[1]/delta),
		    (int) (Loffset[2]/delta)};

  for (int i=nsubx0; i<nsubx; i++){
    for (int j=nsuby0; j<nsuby; j++){
      for (int k=nsubz0; k<nsubz; k++){
	for (int ip=0; ip<nip; ip++) {

	  if (l>=np_local) {
	    cout << "Error in Solid::populate(), exceeding the allocated number of particles.\n";
	    cout << "l = " << l << endl;
	    cout << "np_local = " << np_local << endl;
	    error->all(FLERR, "");
	  }

	  x0[l][0] = x[l][0] = boundlo[0] + delta*(noffset[0] + i + 0.5 + intpoints[3*ip+0]);
	  x0[l][1] = x[l][1] = boundlo[1] + delta*(noffset[1] + j + 0.5 + intpoints[3*ip+1]);
	  if (dim == 3) x0[l][2] = x[l][2] = boundlo[2] + delta*(noffset[2] + k + 0.5 + intpoints[3*ip+2]);
	  else x0[l][2] = x[l][2] = 0;

	  // Check if the particle is inside the region:
	  if (domain->inside_subdomain(x0[l]) && domain->regions[iregion]->inside(x0[l][0], x0[l][1], x0[l][2])==1) {

	    if (update->method->is_CPDI && nc != 0) {
	      rp0[dim*l][0] = rp[dim*l][0] = lp;
	      rp0[dim*l][1] = rp[dim*l][1] = 0;
	      rp0[dim*l][2] = rp[dim*l][2] = 0;

	      if (dim >= 2) {
		rp0[dim*l+1][0] = rp[dim*l+1][0] = 0;
		rp0[dim*l+1][1] = rp[dim*l+1][1] = lp;
		rp0[dim*l+1][2] = rp[dim*l+1][2] = 0;

		if (dim == 3) {
		  rp0[dim*l+2][0] = rp[dim*l+1][0] = 0;
		  rp0[dim*l+2][1] = rp[dim*l+1][1] = 0;
		  rp0[dim*l+2][0] = rp[dim*l+1][0] = lp;
		}
	      }
	    }

#ifdef DEBUG
	    x2plot.push_back(x0[l][0]);
	    y2plot.push_back(x0[l][1]);
#endif
	    l++;
	  }
	}
      }
    }
  }

  MPI_Barrier(universe->uworld);

#ifdef DEBUG
  vector<double> xdomain, ydomain;
  vector<double> xsubdomain, ysubdomain;

  xdomain.push_back(domain->boxlo[0]);
  ydomain.push_back(domain->boxlo[1]);

  xdomain.push_back(domain->boxhi[0]);
  ydomain.push_back(domain->boxlo[1]);

  xdomain.push_back(domain->boxhi[0]);
  ydomain.push_back(domain->boxhi[1]);

  xdomain.push_back(domain->boxlo[0]);
  ydomain.push_back(domain->boxhi[1]);

  xdomain.push_back(domain->boxlo[0]);
  ydomain.push_back(domain->boxlo[1]);


  xsubdomain.push_back(domain->sublo[0]);
  ysubdomain.push_back(domain->sublo[1]);

  xsubdomain.push_back(domain->subhi[0]);
  ysubdomain.push_back(domain->sublo[1]);

  xsubdomain.push_back(domain->subhi[0]);
  ysubdomain.push_back(domain->subhi[1]);

  xsubdomain.push_back(domain->sublo[0]);
  ysubdomain.push_back(domain->subhi[1]);

  xsubdomain.push_back(domain->sublo[0]);
  ysubdomain.push_back(domain->sublo[1]);

  plt::figure_size(1200, 780);
  plt::plot(xdomain, ydomain, "b-");
  plt::plot(xsubdomain, ysubdomain, "r-");
  plt::plot(x2plot, y2plot, ".");
  plt::axis("equal");
  plt::save("debug-proc_" + to_string(universe->me) + ".png");
  plt::close();
#endif

  np_local = l; // Adjust np to account for the particles outside the domain

#ifdef DEBUG
  cout << "proc " << universe->me << "\tnp_local=" << np_local << endl;
#endif

  MPI_Allreduce(&np_local,&np,1,MPI_MPM_BIGINT,MPI_SUM,universe->uworld);

  tagint ptag0 = 0;

  for (int proc=0; proc<universe->nprocs; proc++){
    int np_local_bcast;
    if (proc == universe->me) {
      // Send np_local
      np_local_bcast = np_local;
    } else {
      // Receive np_local
      np_local_bcast = 0;
    }
    MPI_Bcast(&np_local_bcast,1,MPI_INT,proc,universe->uworld);
    if (universe->me > proc) ptag0 += np_local_bcast;
  }

#ifdef DEBUG
  cout << "proc " << universe->me << "\tptag0 = " << ptag0 << endl;
#endif


  for (int i=0; i<np_local;i++) {
    a[i].setZero();
    v[i].setZero();
    f[i].setZero();
    mb[i].setZero();
    v_update[i].setZero();
    vol0[i] = vol[i] = vol_;
    rho0[i] = rho[i] = mat->rho0;
    mass[i] = mass_;
    eff_plastic_strain[i] = 0;
    eff_plastic_strain_rate[i] = 0;
    damage[i] = 0;
    damage_init[i] = 0;
    strain_el[i].setZero();
    sigma[i].setZero();
    vol0PK1[i].setZero();
    L[i].setZero();
    F[i].setIdentity();
    R[i].setIdentity();
    U[i].setZero();
    D[i].setZero();
    Finv[i].setZero();
    Fdot[i].setZero();
    Di[i].setZero();

    J[i] = 1;

    ptag[i] = ptag0 + i + 1;
  }
  error->all(FLERR, "");
}

void Solid::update_particle_domain() {
  int dim = domain->dimension;

  for (int ip=0; ip<np; ip++){
    rp[dim*ip] = F[ip]*rp0[dim*ip];
    if (dim >= 2) rp[dim*ip+1] = F[ip]*rp0[dim*ip+1];
    if (dim == 3) rp[dim*ip+2] = F[ip]*rp0[dim*ip+2];
  }
}

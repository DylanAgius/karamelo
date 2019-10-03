/* -*- c++ -*- ----------------------------------------------------------*/

#ifndef MPM_SOLID_H
#define MPM_SOLID_H

#include "pointers.h"
#include "material.h"
#include "grid.h"
#include <vector>
#include <Eigen/Eigen>


using namespace Eigen;

class Solid : protected Pointers {
 public:
  string id;                 // solid id
  bigint np;                 // total number of particles in the domain
  int np_local;              // number of local particles (in this CPU)

  vector<tagint> ptag;              // unique identifier for particles in the system

  double solidlo[3], solidhi[3];           // solid global bounds
  double solidsublo[3], solidsubhi[3];     // solid local bounds

  vector<Eigen::Vector3d> x;        // particles' current position
  vector<Eigen::Vector3d> x0;       // particles' reference position

  
  vector<Eigen::Vector3d> rp;       // current domain vector (CPDI1)
  vector<Eigen::Vector3d> rp0;      // reference domain vector (CPDI1)
  vector<Eigen::Vector3d> xpc;      // current position of the corners of the particles' domain (CPDI2)
  vector<Eigen::Vector3d> xpc0;     // reference position of the corners of the particles' domain (CPDI2)
  int nc;                           // number of corners per particles: 2^dimension
  
  vector<Eigen::Vector3d> v;        // particles' current velocity
  vector<Eigen::Vector3d> v_update; // particles' velocity at time t+dt

  vector<Eigen::Vector3d> a;         // particles' acceleration

  vector<Eigen::Vector3d> mbp;       // particles' external forces times mass
  vector<Eigen::Vector3d> f;         // particles' internal forces

  vector<Eigen::Matrix3d> sigma;            // stress matrix
  vector<Eigen::Matrix3d> strain_el;        // elastic strain matrix
  vector<Eigen::Matrix3d> vol0PK1;          // Transpose of the 1st Piola-Kirchhoff matrix times vol0
  vector<Eigen::Matrix3d> L;                // velocity gradient matrix
  vector<Eigen::Matrix3d> F;                // deformation gradient matrix
  vector<Eigen::Matrix3d> R;                // Rotation matrix
  vector<Eigen::Matrix3d> U;
  vector<Eigen::Matrix3d> D;                // symmetric part of L
  vector<Eigen::Matrix3d> Finv;             // inverse of the deformation gradient matrix
  vector<Eigen::Matrix3d> Fdot;             // rate of deformation gradient matrix
  vector<Eigen::Matrix3d> Di;               // inertia tensor

  vector<double> J;                         // determinant of the deformation matrix
  vector<double> vol0;                      // particles' reference volume
  vector<double> vol;                       // particles' current volume
  double vtot;                              // total volume
  vector<double> rho0;                      // particles' reference density
  vector<double> rho;                       // particles' current density
  vector<double> mass;                      // particles' current mass
  vector<double> eff_plastic_strain;        // particles' effective plastic strain
  vector<double> eff_plastic_strain_rate;   // particles' effective plastic strain rate
  vector<double> damage;                    // particles' damage variable
  vector<double> damage_init;               // particles' damage initiation variable
  vector<int> mask;                         // particles' group mask

  double min_inv_p_wave_speed;   // minimum of the inverse of the particle wave speed
  double dtCFL;
  
  int *numneigh_pn;   // number of nodes neighbouring a given particle
  int *numneigh_np;   // number of nodes neighbouring a given node
  vector<int> *neigh_pn;     // List of the nodes neighbouring a given particle
  vector<int> *neigh_np;     // List of the particles neighbouring a given node

  vector< double > *wf_pn;
  vector< double > *wf_np;
  vector< Eigen::Vector3d > *wfd_pn;
  vector< Eigen::Vector3d > *wfd_np;

  struct Mat *mat;                     // Material

  class Grid *grid;                   // background grid

  string method_style;

  Solid(class MPM *, vector<string>);
  virtual ~Solid();

  void init();
  void options(vector<string> *, vector<string>::iterator);
  void grow(int);

  void compute_mass_nodes(bool);
  void compute_velocity_nodes(bool);
  void compute_velocity_nodes_APIC(bool);
  void compute_external_forces_nodes(bool);
  void compute_internal_forces_nodes_TL();
  void compute_internal_forces_nodes_UL(bool);
  void compute_particle_velocities();
  void compute_particle_acceleration();
  void update_particle_position();
  void update_particle_velocities(double);
  void compute_rate_deformation_gradient_TL();
  void compute_rate_deformation_gradient_TL_APIC();
  void compute_rate_deformation_gradient_UL_USL();
  void compute_rate_deformation_gradient_UL_MUSL();
  void compute_rate_deformation_gradient_UL_APIC();
  void update_deformation_gradient();
  void update_stress();
  void compute_inertia_tensor(string);
  void compute_deformation_gradient();
  void update_particle_domain();

private:
  void populate(vector<string>);
};

#endif

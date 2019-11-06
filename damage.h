/* -*- c++ -*- ----------------------------------------------------------*/

#ifndef MPM_DAMAGE_H
#define MPM_DAMAGE_H

#include "pointers.h"
#include <vector>
#include <Eigen/Eigen>

class Damage : protected Pointers {
 public:
  string id;

  Damage(MPM *, vector<string>);
  virtual ~Damage();
  virtual void init();
  void options(vector<string> *, vector<string>::iterator);

  // implemented by each Damage
  virtual void compute_damage(double &damage_init,
			      double &damage,
			      const double pH,
			      const Eigen::Matrix3d Sdev,
			      const double epsdot,
			      const double plastic_strain_increment,
			      const double temperature) = 0;
};

#endif

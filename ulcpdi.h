/* -*- c++ -*- ----------------------------------------------------------*/

#ifdef METHOD_CLASS

MethodStyle(ulcpdi,ULCPDI)

#else

#ifndef LMP_ULCPDI_H
#define LMP_ULCPDI_H

#include "method.h"
#include <vector>
#include <Eigen/Eigen>
#include <map>


class ULCPDI : public Method {
 public:
  string method_type;
  double FLIP;
  string shape_function;
  int style;

  ULCPDI(class MPM *, vector<string>);
  ~ULCPDI();

  void setup(vector<string>);

  void compute_grid_weight_functions_and_gradients();
  double (*basis_function)(double, int);
  double (*derivative_basis_function)(double, int, double);
  void particles_to_grid();
  void update_grid_state();
  void grid_to_points();
  void advance_particles();
  void velocities_to_grid();
  void update_grid_positions() {};
  void compute_rate_deformation_gradient();
  void update_deformation_gradient();
  void update_stress();
  void adjust_dt();
  void reset();

  int update_wf;
private:
  string known_styles[2] = {"R4", "Q4"};
};

// double linear_basis_function(double, int);
// double derivative_linear_basis_function(double, int, double);
// double cubic_spline_basis_function(double, int);
// double derivative_cubic_spline_basis_function(double, int, double);
// double bernstein_quadratic_basis_function(double, int);
// double derivative_bernstein_quadratic_basis_function(double, int, double);


#endif
#endif

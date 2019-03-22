#include <iostream>
#include "strength_jc.h"
#include "input.h"
#include "domain.h"
#include "update.h"
#include "mpm_math.h"
#include <Eigen/Eigen>
#include "var.h"

using namespace std;
using namespace Eigen;
using namespace MPM_Math;


StrengthJohnsonCook::StrengthJohnsonCook(MPM *mpm, vector<string> args) : Strength(mpm, args)
{
  cout << "Initiate StrengthJohnsonCook" << endl;

  if (args.size()<8) {
    cout << "Error: too few arguments for the strength command" << endl;
    exit(1);
  }
  //options(&args, args.begin()+3);
  G_ = input->parsev(args[2]);
  A = input->parsev(args[3]);
  B = input->parsev(args[4]);
  n = input->parsev(args[5]);
  epsdot0 = input->parsev(args[6]);
  C = input->parsev(args[7]);

  cout << "Johnson Cook material strength model:\n";
  cout << "\tG: shear modulus " << G_ << endl;
  cout << "\tA: initial yield stress " << A << endl;
  cout << "\tB: proportionality factor for plastic strain dependency " << B << endl;
  cout << "\tn: exponent for plastic strain dependency " << n << endl;
  cout << "\tepsdot0: reference strain rate " << epsdot0 << endl;
  cout << "\tC: proportionality factor for logarithmic plastic strain rate dependency " << C << endl;
}

double StrengthJohnsonCook::G(){
  return G_;
}

Matrix3d StrengthJohnsonCook::update_deviatoric_stress(const Matrix3d sigma, const Matrix3d D, double &plastic_strain_increment, const double eff_plastic_strain, const double epsdot)
{
  Matrix3d sigmaInitial_dev, sigmaFinal_dev, sigmaTrial_dev, dev_rate;
  double J2, yieldStress;

  double epsdot_ratio = epsdot / epsdot0;
  epsdot_ratio = MAX(epsdot_ratio, 1.0);
	
  yieldStress = (A + B * pow(eff_plastic_strain, n)) * pow(1.0 + epsdot_ratio, C); // * (1.0 - pow(TH, M));
  /*
   * deviatoric rate of unrotated stress
   */
  dev_rate = 2.0 * G_ * Deviator(D);
  sigmaInitial_dev = Deviator(sigma);

  /*
   * perform a trial elastic update to the deviatoric stress
   */
  sigmaTrial_dev = sigmaInitial_dev + update->dt * dev_rate; // increment stress deviator using deviatoric rate

  /*
   * check yield condition
   */
  J2 = sqrt(3. / 2.) * sigmaTrial_dev.norm();
  sigmaFinal_dev = sigmaTrial_dev;

  if (J2 < yieldStress) {
    /*
     * no yielding has occured.
     * final deviatoric stress is trial deviatoric stress
     */
    plastic_strain_increment = 0.0;

  } else {
    //printf("yiedl\n");
    /*
     * yielding has occured
     */

    plastic_strain_increment = (J2 - yieldStress) / (3.0 * G_);
    /*
     * new deviatoric stress:
     * obtain by scaling the trial stress deviator
     */
    sigmaFinal_dev *= (yieldStress / J2);

  }

  return sigmaFinal_dev;
}
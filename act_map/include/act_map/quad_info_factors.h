#pragma once

#include <rpg_common/pose.h>
#include <rpg_common/eigen_type.h>
#include <vi_utils/vi_jacobians.h>

#include "act_map/info_utils.h"
#include "act_map/sampler.h"

namespace act_map
{
using InfoKValueType = double;
using InfoK1 = Eigen::Matrix<InfoKValueType, 18, 18>;
using InfoK1Vec = rpg::Aligned<std::vector, InfoK1>;

using InfoK2 = Eigen::Matrix<InfoKValueType, 18, 6>;
using InfoK2Vec = rpg::Aligned<std::vector, InfoK2>;

using InfoK3 = Eigen::Matrix<InfoKValueType, 6, 6>;
using InfoK3Vec = rpg::Aligned<std::vector, InfoK3>;

using Info = Eigen::Matrix<InfoKValueType, 6, 6>;
using InfoVec = rpg::Aligned<std::vector, Info>;
}

namespace act_map
{
template <typename F>
void manipulateFactorSingle(const Eigen::Vector3d& pw,
                            const Eigen::Vector3d& twc, InfoK1* K1, InfoK2* K2,
                            InfoK3* K3)
{
  Eigen::Vector3d p0 = pw - twc;
  Eigen::Matrix3d p0p0T = p0 * p0.transpose();
  double d0 = p0.norm();

  rpg::Rotation rot;
  rot.setIdentity();
  rpg::Matrix36 J0 =
      vi_utils::jacobians::dBearing_dIMUPoseGlobal(pw, rpg::Pose(rot, -twc));
  rpg::Matrix66 H0 = J0.transpose() * J0;
  rpg::Matrix66 H0_over_d = H0 / d0;
  rpg::Matrix66 H0_over_d2 = H0_over_d / d0;

  F func;
  for (int ri = 0; ri < 6; ri++)
  {
    for (int ci = 0; ci < 6; ci++)
    {
      Eigen::Matrix3d v1 = p0p0T * H0_over_d2(ri, ci);
      Eigen::Block<InfoK1, 3, 3> b1 = K1->block<3, 3>(ri * 3, ci * 3);
      Eigen::Matrix<InfoKValueType, 3, 3> v1k = v1.cast<InfoKValueType>();
      func.operator()(b1, v1k);

      Eigen::Vector3d v2 = p0 * H0_over_d(ri, ci);
      Eigen::Block<InfoK2, 3, 1> b2 = K2->block<3, 1>(ri * 3, ci);
      Eigen::Matrix<InfoKValueType, 3, 1> v2k = v2.cast<InfoKValueType>();
      func.operator()(b2, v2k);
    }
  }
  Eigen::Matrix<InfoKValueType, 6, 6> H0k = H0.cast<InfoKValueType>();
  func.operator()(*K3, H0k);
}

void getInfoAtRotation(const Eigen::Matrix3d& Rwc, const double k1,
                       const double k2, const double k3, const InfoK1& K1,
                       const InfoK2& K2, const InfoK3& K3,
                       Eigen::Matrix<double, 6, 6>* H);

// overloaded functions
inline double getInfoMetricAtRotationFromPositionalFactor(
    const Eigen::Matrix3d& Rwc, const double k1, const double k2,
    const double k3, const InfoK1& K1, const InfoK2& K2, const InfoK3& K3,
    const InfoMetricType& v, Eigen::Vector3d* drot_global = nullptr)
{
  Eigen::Matrix<double, 6, 6> H;
  getInfoAtRotation(Rwc, k1, k2, k3, K1, K2, K3, &H);

  static double dtheta = (1.0 / 180.0) * M_PI;
  if (drot_global)
  {
    for (int i = 0; i < 3; i++)
    {
      Eigen::Matrix3d Rwcm;
      Eigen::Matrix3d Rwcp;
      utils::getRotationPerturbTwoSides(Rwc, dtheta, i, &Rwcm, &Rwcp);

      double valm = getInfoMetricAtRotationFromPositionalFactor(Rwcm, k1, k2, k3, K1, K2,
                                                      K3, v, nullptr);
      double valp = getInfoMetricAtRotationFromPositionalFactor(Rwcp, k1, k2, k3, K1, K2,
                                                      K3, v, nullptr);
      (*drot_global)(i) = (valp - valm) / (2 * dtheta);
    }
  }
  return getInfoMetric(H, v);
}
}

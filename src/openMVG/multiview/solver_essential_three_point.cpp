// This file is part of OpenMVG, an Open Multiple View Geometry C++ library.

// Copyright (c) 2017, Romain JANVIER & Pierre MOULON
// Copyright (c) 2020 Pierre MOULON.

// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <cmath>
#include "openMVG/multiview/solver_essential_three_point.hpp"

namespace openMVG {

//
//
// Three point orthographic essential matrix estimation
// This solver is based on the following paper:
// Magnus Oskarsson, "Two-View Orthographic Epipolar Geometry: Minimal and Optimal Solvers",
// Journal of Mathematical Imaging and Vision, 2017
//
// Original matlab implementation can be found online
// https://github.com/hamburgerlady/ortho-gem
/**
 * @brief Computes the relative pose of two orthographic cameras from 3 correspondences.
 *
 * \param x1 Points in the first image. One per column.
 * \param x2 Corresponding points in the second image. One per column.
 * \param E  A list of at most 2 candidates orthographic essential matrix solutions.
 */
void ThreePointsRelativePose
(
  const Mat2X &x1,
  const Mat2X &x2,
  std::vector<Mat3> *E
)
{
  const Vec2
    xd1 = x1.col(1) - x1.col(0),
    yd1 = x1.col(2) - x1.col(0),
    xd2 = x2.col(1) - x2.col(0),
    yd2 = x2.col(2) - x2.col(0);

  const double
    denom = xd1.x() * yd1.y() - xd1.y() * yd1.x(),
    aac = ( xd1.y() * yd2.x() - xd2.x() * yd1.y() ) / denom,
    aad = ( xd1.y() * yd2.y() - xd2.y() * yd1.y() ) / denom,
    bbc = ( xd2.x() * yd1.x() - xd1.x() * yd2.x() ) / denom,
    bbd = ( xd2.y() * yd1.x() - xd1.x() * yd2.y() ) / denom;

  // cache aac * aac
  const double aac_sq = aac * aac;

  const double
    dd_2 = - aac_sq + aad * aad - bbc * bbc + bbd * bbd,
    dd_1c = 2.0 * aac * aad + 2.0 * bbc * bbd,
    dd_0 = aac_sq + bbc * bbc - 1.0,
    d4_4 = dd_1c * dd_1c + dd_2 * dd_2,
    d4_2 = -dd_1c * dd_1c + 2.0 * dd_0 * dd_2,
    d4_0 = dd_0 * dd_0,
    tmp = sqrt( ( d4_2 * d4_2 - 4.0 * d4_4 * d4_0 ) ),
    tmp_csol = - aac_sq + aad * aad - bbc * bbc + bbd * bbd;

  const auto ComputeEssentialMatrix = [&](const double root) -> Mat3 {
    const double
      dsol = sqrt( - root / d4_4 / 2.0 ),
      csol = - ( tmp_csol * dsol * dsol + aac_sq + bbc * bbc - 1.0 ) /
                 ( 2.0 * aac * aad * dsol + 2.0 * bbc * bbd * dsol ),
      asol = aac * csol + aad * dsol,
      bsol = bbc * csol + bbd * dsol,
      esol = - asol * x1(0,0) - bsol * x1(1,0) - csol * x2(0,0)
             - dsol * x2(1,0);

    return (Mat3() << 0.0,   0.0, asol,
                      0.0,   0.0, bsol,
                      csol, dsol, esol).finished();
  };

  E->emplace_back(ComputeEssentialMatrix(d4_2 + tmp));
  E->emplace_back(ComputeEssentialMatrix(d4_2 - tmp));
}

namespace essential {
namespace kernel {

void ThreePointUprightRelativePoseSolver::Solve
(
  const Mat3X & bearing_a,
  const Mat3X & bearing_b,
  std::vector<Mat3> * pvec_E
)
{
  // Build the action matrix -> see (6,7) in the paper
  Eigen::Matrix<double, 3, 4> A;
  for (const int i : {0,1,2})
  {
    const auto & bearing_a_i = bearing_a.col(i);
    const auto & bearing_b_i = bearing_b.col(i);
    A.row(i) <<
         bearing_a_i.x() * bearing_b_i.y(), -bearing_a_i.z() * bearing_b_i.y(),
        -bearing_b_i.x() * bearing_a_i.y(), -bearing_b_i.z() * bearing_a_i.y();
  }

  Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double, 4, 4>> solver(A.transpose() * A);
  const Vec4 nullspace = solver.eigenvectors().leftCols<1>();

  Mat3 E = Mat3::Zero(); // see (3) in the paper
  E(0, 1) =   nullspace(2);
  E(1, 0) = - nullspace(0);
  E(1, 2) =   nullspace(1);
  E(2, 1) =   nullspace(3);

  pvec_E->emplace_back(E);
}

} // namespace kernel
} // namespace essential
} // namespace openMVG

// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "thm_core/TransientSolver.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <Eigen/Sparse>

#include "thm_core/SteadySolver.h"

// Backward-Euler time integration of  C dT/dt + G T = P. The conduction matrix
// G and the boundary/load vector b are reused from the steady assembly, so the
// discretization (harmonic half-cell conductances, isothermal bottom boundary)
// is identical to the steady solver; only the diagonal capacitance term and the
// time stepping are added here. Backward-Euler is chosen for unconditional
// stability (any dt is stable), and its fixed-point is exactly the steady
// solution, which the tests check.

namespace thm::core {

TransientResult solveTransient(const SteadyProblem& p,
                               double dt_s,
                               int num_steps,
                               double t_init_celsius)
{
  if (!(dt_s > 0.0)) {
    throw std::invalid_argument("thm_core: dt_s must be > 0");
  }
  if (num_steps < 1) {
    throw std::invalid_argument("thm_core: num_steps must be >= 1");
  }
  for (const Layer& layer : p.layers) {
    if (!(layer.volumetric_heat_capacity_j_per_m3k > 0.0)) {
      throw std::invalid_argument(
          "thm_core: transient requires volumetric_heat_capacity > 0 per layer");
    }
  }

  // Conduction matrix G and load vector b (power + Dirichlet boundary term).
  // assembleSystem() also validates the problem geometry/sizes.
  CsrMatrix g;
  std::vector<double> b;
  assembleSystem(p, g, b);
  const int n = g.n;

  // Lumped diagonal heat capacity over dt: (C/dt)_cell = (rho*c_p)_l * dx*dy*t_l / dt.
  const int nxy = p.nx * p.ny;
  std::vector<double> c_over_dt(n, 0.0);
  int offset = 0;
  for (const Layer& layer : p.layers) {
    const double cell_capacity = layer.volumetric_heat_capacity_j_per_m3k
                                 * (p.dx_m * p.dy_m * layer.thickness_m);
    const double value = cell_capacity / dt_s;
    for (int c = 0; c < nxy; ++c) {
      c_over_dt[offset + c] = value;
    }
    offset += nxy;
  }

  // Constant system matrix A = G + diag(C/dt); factor once.
  Eigen::SparseMatrix<double> a(n, n);
  std::vector<Eigen::Triplet<double>> triplets;
  triplets.reserve(g.values.size() + n);
  for (int r = 0; r < n; ++r) {
    for (int k = g.row_ptr[r]; k < g.row_ptr[r + 1]; ++k) {
      triplets.emplace_back(r, g.col_idx[k], g.values[k]);
    }
  }
  for (int r = 0; r < n; ++r) {
    triplets.emplace_back(r, r, c_over_dt[r]);  // summed into the G diagonal
  }
  a.setFromTriplets(triplets.begin(), triplets.end());
  a.makeCompressed();

  Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
  solver.compute(a);
  if (solver.info() != Eigen::Success) {
    throw std::runtime_error("thm_core: transient factorization failed");
  }

  Eigen::VectorXd t_cur(n);
  t_cur.setConstant(t_init_celsius);

  auto peak_of = [n](const Eigen::VectorXd& v) {
    double m = v[0];
    for (int i = 1; i < n; ++i) {
      m = std::max(m, v[i]);
    }
    return m;
  };

  TransientResult res;
  res.peak_history_celsius.reserve(static_cast<std::size_t>(num_steps) + 1);
  res.peak_history_celsius.push_back(peak_of(t_cur));

  Eigen::VectorXd rhs(n);
  for (int step = 0; step < num_steps; ++step) {
    for (int i = 0; i < n; ++i) {
      rhs[i] = c_over_dt[i] * t_cur[i] + b[i];  // (C/dt) T^n + b
    }
    t_cur = solver.solve(rhs);
    if (solver.info() != Eigen::Success) {
      throw std::runtime_error("thm_core: transient solve failed");
    }
    res.peak_history_celsius.push_back(peak_of(t_cur));
  }

  res.temperature_celsius.assign(t_cur.data(), t_cur.data() + n);
  res.peak_celsius = peak_of(t_cur);
  return res;
}

}  // namespace thm::core

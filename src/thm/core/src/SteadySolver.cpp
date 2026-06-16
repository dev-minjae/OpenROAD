// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "thm_core/SteadySolver.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <Eigen/Sparse>

// Discretization (independently derived from standard heat-transfer theory; no
// third-party thermal-tool source was copied):
//
//  - Steady conduction  div(k grad T) + q = 0  on a cell-centered finite-volume
//    grid. Integrating over a control volume gives, per cell, the balance
//    sum_neighbors G_ab (T_b - T_a) + P_a = 0, i.e. a thermal resistor network
//    (Fourier's law per face).
//  - Face conductance between two adjacent cells = the series combination of the
//    two half-cell conduction resistances:
//        G_ab = A / ( d_a/(2 k_a) + d_b/(2 k_b) )
//    This is the harmonic mean of the half-cell conductivities -- the correct
//    interface treatment for heterogeneous materials (Patankar, "Numerical Heat
//    Transfer and Fluid Flow", 1980, sec. 4.2-4.3). The arithmetic mean is wrong
//    across a material interface and is deliberately avoided.
//  - Dirichlet (isothermal) boundary: a half-cell conductance from the boundary
//    cell center to the fixed-temperature surface,  G_bc = A / (d/(2 k)), added
//    to the diagonal with G_bc*T_bc moved to the right-hand side. This grounds
//    the otherwise-singular all-Neumann system, making G symmetric positive
//    definite.

namespace thm::core {

namespace {

inline int cellIndex(int i, int j, int l, int nx, int ny)
{
  return l * nx * ny + j * nx + i;
}

// Series conductance of two half-cells sharing a face of area `area` [W/K].
inline double seriesConductance(double k_a,
                                double d_a,
                                double k_b,
                                double d_b,
                                double area)
{
  return area / (d_a / (2.0 * k_a) + d_b / (2.0 * k_b));
}

// Half-cell conductance from a cell center to a fixed-temperature surface [W/K].
inline double halfCellConductance(double k, double d, double area)
{
  return area / (d / (2.0 * k));
}

void validate(const SteadyProblem& p)
{
  if (p.layers.empty()) {
    throw std::invalid_argument("thm_core: problem has no layers");
  }
  if (p.nx <= 0 || p.ny <= 0) {
    throw std::invalid_argument("thm_core: nx and ny must be > 0");
  }
  if (!(p.dx_m > 0.0) || !(p.dy_m > 0.0)) {
    throw std::invalid_argument("thm_core: dx_m and dy_m must be > 0");
  }
  for (const Layer& layer : p.layers) {
    if (!(layer.thickness_m > 0.0) || !(layer.conductivity_w_per_mk > 0.0)) {
      throw std::invalid_argument(
          "thm_core: layer thickness and conductivity must be > 0");
    }
  }
  const std::size_t need
      = static_cast<std::size_t>(p.nx) * p.ny * p.layers.size();
  if (p.power_w.size() != need) {
    throw std::invalid_argument("thm_core: power_w size != nx*ny*layers");
  }
}

}  // namespace

void assembleSystem(const SteadyProblem& p,
                    CsrMatrix& g,
                    std::vector<double>& rhs)
{
  validate(p);

  const int nx = p.nx;
  const int ny = p.ny;
  const int nz = static_cast<int>(p.layers.size());
  const int n = nx * ny * nz;
  const double area_z = p.dx_m * p.dy_m;  // face normal to z

  std::vector<double> diag(n, 0.0);
  std::vector<std::vector<std::pair<int, double>>> off(n);
  rhs.assign(n, 0.0);

  auto addEdge = [&](int a, int b, double cond) {
    off[a].emplace_back(b, -cond);
    off[b].emplace_back(a, -cond);
    diag[a] += cond;
    diag[b] += cond;
  };

  for (int l = 0; l < nz; ++l) {
    const double t = p.layers[l].thickness_m;
    const double k = p.layers[l].conductivity_w_per_mk;
    const double area_x = p.dy_m * t;  // face normal to x
    const double area_y = p.dx_m * t;  // face normal to y
    for (int j = 0; j < ny; ++j) {
      for (int i = 0; i < nx; ++i) {
        const int c = cellIndex(i, j, l, nx, ny);
        rhs[c] += p.power_w[c];

        if (i + 1 < nx) {  // +x neighbor (same layer)
          addEdge(c,
                  cellIndex(i + 1, j, l, nx, ny),
                  seriesConductance(k, p.dx_m, k, p.dx_m, area_x));
        }
        if (j + 1 < ny) {  // +y neighbor (same layer)
          addEdge(c,
                  cellIndex(i, j + 1, l, nx, ny),
                  seriesConductance(k, p.dy_m, k, p.dy_m, area_y));
        }
        if (l + 1 < nz) {  // +z neighbor (next layer up; heterogeneous)
          const double t2 = p.layers[l + 1].thickness_m;
          const double k2 = p.layers[l + 1].conductivity_w_per_mk;
          addEdge(c,
                  cellIndex(i, j, l + 1, nx, ny),
                  seriesConductance(k, t, k2, t2, area_z));
        }
        if (l == 0) {  // bottom isothermal boundary
          const double g_bc = halfCellConductance(k, t, area_z);
          diag[c] += g_bc;
          rhs[c] += g_bc * p.t_bc_celsius;
        }
      }
    }
  }

  // Flatten to CSR with sorted columns.
  g.n = n;
  g.row_ptr.assign(n + 1, 0);
  g.col_idx.clear();
  g.values.clear();
  for (int r = 0; r < n; ++r) {
    std::vector<std::pair<int, double>> row = std::move(off[r]);
    row.emplace_back(r, diag[r]);
    std::sort(row.begin(), row.end());
    g.row_ptr[r + 1] = g.row_ptr[r] + static_cast<int>(row.size());
    for (const auto& [col, val] : row) {
      g.col_idx.push_back(col);
      g.values.push_back(val);
    }
  }
}

SteadyResult solveSteady(const SteadyProblem& p)
{
  CsrMatrix g;
  std::vector<double> rhs;
  assembleSystem(p, g, rhs);
  const int n = g.n;

  Eigen::SparseMatrix<double> a(n, n);
  std::vector<Eigen::Triplet<double>> triplets;
  triplets.reserve(g.values.size());
  for (int r = 0; r < n; ++r) {
    for (int k = g.row_ptr[r]; k < g.row_ptr[r + 1]; ++k) {
      triplets.emplace_back(r, g.col_idx[k], g.values[k]);
    }
  }
  a.setFromTriplets(triplets.begin(), triplets.end());
  a.makeCompressed();

  Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
  solver.compute(a);
  if (solver.info() != Eigen::Success) {
    throw std::runtime_error("thm_core: matrix factorization failed");
  }
  Eigen::VectorXd b(n);
  for (int i = 0; i < n; ++i) {
    b[i] = rhs[i];
  }
  const Eigen::VectorXd x = solver.solve(b);
  if (solver.info() != Eigen::Success) {
    throw std::runtime_error("thm_core: linear solve failed");
  }

  SteadyResult res;
  res.temperature_celsius.resize(n);
  double peak = x[0];
  for (int i = 0; i < n; ++i) {
    res.temperature_celsius[i] = x[i];
    peak = std::max(peak, x[i]);
  }
  double total_power = 0.0;
  for (const double w : p.power_w) {
    total_power += w;
  }
  res.peak_celsius = peak;
  res.r_th_k_per_w
      = (total_power != 0.0) ? (peak - p.t_bc_celsius) / total_power : 0.0;
  return res;
}

}  // namespace thm::core

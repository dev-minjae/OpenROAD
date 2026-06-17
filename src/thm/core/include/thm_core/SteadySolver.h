// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <vector>

// Minimal steady-state finite-difference thermal conduction solver.
//
// Pure engine: NO OpenROAD headers; public I/O is plain structs/std::vector so
// the numerical backend (Eigen today, Kokkos at T5) stays swappable and the
// engine remains independently buildable/testable.
//
// Units are SI throughout:
//   length  [m]      conductivity [W/(m*K)]
//   power   [W]      temperature  [degC]   (Celsius; conduction is linear so the
//                                           boundary temperature just sets the
//                                           offset)
//
// Model: a cell-centered structured grid, nx*ny in-plane (uniform dx, dy) times
// N layers stacked in z. Each layer is one cell thick in z (its thickness t_l).
// Cell index ordering is  idx = l*(nx*ny) + j*nx + i,  with
//   i in [0,nx), j in [0,ny), l in [0,N)  (l = 0 is the bottom layer).
//
// Boundary conditions: the bottom face of layer 0 is isothermal (Dirichlet) at
// t_bc; every other outer face is adiabatic (zero-flux Neumann).
namespace thm::core {

// One material layer of the vertical stack.
struct Layer
{
  double thickness_m;            // t_l
  double conductivity_w_per_mk;  // k_l
  // Volumetric heat capacity rho*c_p [J/(m^3*K)]. Used only by the transient
  // solver; the steady solver ignores it, so it defaults to 0 and existing
  // {thickness, conductivity} initializers stay valid.
  double volumetric_heat_capacity_j_per_m3k = 0.0;
};

struct SteadyProblem
{
  std::vector<Layer> layers;  // bottom (on heat sink) -> top
  int nx = 1;
  int ny = 1;
  double dx_m = 0.0;
  double dy_m = 0.0;
  // Per-cell heat generation, size nx*ny*layers.size(), ordering as above.
  std::vector<double> power_w;
  // Isothermal boundary temperature at the bottom face of layer 0.
  double t_bc_celsius = 0.0;
};

struct SteadyResult
{
  std::vector<double> temperature_celsius;  // size nx*ny*N, ordering as above
  double peak_celsius = 0.0;
  double r_th_k_per_w = 0.0;  // (peak - t_bc) / total injected power
};

// Plain compressed-sparse-row matrix. No solver-backend types are exposed, so
// assembly is decoupled from the linear-solve backend.
struct CsrMatrix
{
  int n = 0;
  std::vector<int> row_ptr;    // size n+1
  std::vector<int> col_idx;    // size nnz
  std::vector<double> values;  // size nnz
};

// Assemble the symmetric positive-definite conductance system G*T = rhs.
// Exposed (backend-agnostic) for inspection and testing. Throws
// std::invalid_argument on malformed input.
void assembleSystem(const SteadyProblem& problem,
                    CsrMatrix& g,
                    std::vector<double>& rhs);

// Assemble and solve the steady-state conduction problem. Throws
// std::invalid_argument on malformed input, std::runtime_error if the solve
// fails.
SteadyResult solveSteady(const SteadyProblem& problem);

}  // namespace thm::core

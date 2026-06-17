// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <vector>

#include "thm_core/SteadySolver.h"

// Transient (time-dependent) thermal conduction solver. Same pure-engine rules
// as the steady solver: NO OpenROAD headers, plain struct/array I/O, Eigen used
// only in the .cpp. Reuses SteadyProblem (now with per-layer volumetric heat
// capacity) and the steady conductance assembly.
namespace thm::core {

struct TransientResult
{
  std::vector<double> temperature_celsius;   // final field, size nx*ny*N
  double peak_celsius = 0.0;                  // peak of the final field
  // Peak temperature after each step, size num_steps + 1 (index k <-> t = k*dt,
  // index 0 = the initial condition).
  std::vector<double> peak_history_celsius;
};

// Solve  C dT/dt + G T = P  by backward-Euler with a fixed time step:
//   (C/dt + G) T^{n+1} = (C/dt) T^n + b
// G and the load vector b (power + Dirichlet boundary term) come from the same
// assembleSystem() used by the steady solver. C is the lumped diagonal heat
// capacity, C_cell = (rho*c_p)_layer * (dx*dy*t_layer) [J/K]. Because dt is
// fixed, (C/dt + G) is a constant SPD matrix: it is factorized once and reused
// for every step.
//
// Every layer must set volumetric_heat_capacity_j_per_m3k > 0. The initial
// temperature is t_init_celsius everywhere. Throws std::invalid_argument on
// malformed input, std::runtime_error on solver failure.
TransientResult solveTransient(const SteadyProblem& problem,
                               double dt_s,
                               int num_steps,
                               double t_init_celsius);

}  // namespace thm::core

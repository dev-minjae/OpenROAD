// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "thm/Thermal.h"

#include <vector>

#include "thm_core/SteadySolver.h"
#include "thm_core/ThermalCore.h"

namespace thm {

Thermal::Thermal(odb::dbDatabase* db, utl::Logger* logger)
    : db_(db), logger_(logger)
{
}

void Thermal::analyzeThermal()
{
  // Touch the DB seam (read-only) and the isolated core lib.
  odb::dbChip* chip = db_->getChip();
  const char* design = (chip != nullptr) ? "loaded" : "none";
  logger_->info(utl::THM,
                1,
                "thermal module alive (core {}); design: {}",
                core::version(),
                design);

  // T1 demo: solve a synthetic HBM-like stack with the core steady-state FDM
  // engine. The problem is built by hand here, NOT read from odb (that is T2).
  core::SteadyProblem problem;
  problem.layers = {
      {300e-6, 130.0},  // logic/base die (silicon)
      {20e-6, 0.8},     // underfill / bond layer
      {50e-6, 130.0},   // stacked DRAM die (silicon)
      {20e-6, 0.8},     // underfill / bond layer
      {50e-6, 130.0},   // stacked DRAM die (silicon)
  };
  problem.nx = 8;
  problem.ny = 8;
  problem.dx_m = 100e-6;
  problem.dy_m = 100e-6;
  problem.t_bc_celsius = 25.0;  // heat-sink temperature at the stack bottom

  const int num_layers = static_cast<int>(problem.layers.size());
  const int cells_per_layer = problem.nx * problem.ny;
  problem.power_w.assign(cells_per_layer * num_layers, 0.0);
  // 1 W hot spot in the center of the top die.
  const int top = num_layers - 1;
  const int hot = top * cells_per_layer + (problem.ny / 2) * problem.nx
                  + (problem.nx / 2);
  problem.power_w[hot] = 1.0;

  const core::SteadyResult result = core::solveSteady(problem);
  logger_->info(utl::THM,
                2,
                "T1 steady FDM on synthetic {}-layer stack: peak T = {:.3f} C, "
                "R_th = {:.4f} K/W",
                num_layers,
                result.peak_celsius,
                result.r_th_k_per_w);
}

}  // namespace thm

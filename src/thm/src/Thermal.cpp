// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "thm/Thermal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

#include "odb/unfoldedModel.h"
#include "thm_core/SteadySolver.h"
#include "thm_core/ThermalCore.h"
#include "thm_core/TransientSolver.h"

namespace thm {

namespace {

// --- T2/T3 modeling assumptions. odb carries die geometry only (no materials,
// --- no power), so materials, in-plane grid resolution, boundary temperature,
// --- power and volumetric heat capacity are supplied here. Real per-instance
// --- power and a material database come later; TSV / microbump compact thermal
// --- models are T4. ---
constexpr double kSiliconWPerMK = 130.0;   // die bulk silicon (assumption)
constexpr double kBondWPerMK = 0.8;        // inter-die underfill/bond (assumption)
constexpr double kSiliconVhc = 2330.0 * 700.0;  // rho*c_p ~ 1.63e6 J/(m^3 K), Si
constexpr double kBondVhc = 1500.0 * 1000.0;    // rho*c_p ~ 1.5e6 J/(m^3 K), bond
constexpr int kGridNx = 32;                // in-plane grid resolution (parameter)
constexpr int kGridNy = 32;
constexpr double kHeatSinkCelsius = 25.0;  // bottom isothermal BC (parameter)
constexpr double kPerDieWatts = 1.0;       // synthetic per-die power (parameter)

// Build a SteadyProblem from the odb 3D die stack. Returns false (leaving
// `problem` unset) when there is no usable stack. odb is touched ONLY in this
// file -- thm_core stays OpenROAD-free.
bool buildFromOdbStack(odb::dbDatabase* db,
                       odb::dbChip* chip,
                       utl::Logger* logger,
                       core::SteadyProblem& problem,
                       std::vector<bool>& is_die_layer,
                       int& num_dies)
{
  // DBU -> SI metres via the design's units accessor (NOT a hardcoded 1000;
  // example.3dbx uses precision 2000). All geometry below is raw DBU.
  const double dbu_per_um = static_cast<double>(db->getDbuPerMicron());
  auto toMeters = [dbu_per_um](long long dbu) {
    return (static_cast<double>(dbu) / dbu_per_um) * 1e-6;
  };

  // Collect leaf dies with their global (transform-applied) cuboids.
  odb::UnfoldedModel model(logger, chip);
  struct Die
  {
    odb::Cuboid box;
    std::string name;
  };
  std::vector<Die> dies;
  for (const auto& uf : model.getChips()) {
    if (uf.chip_inst_path.empty()) {
      continue;  // skip the top assembly; keep leaf dies only
    }
    dies.push_back({uf.cuboid, uf.name});
  }
  if (dies.empty()) {
    return false;
  }

  // z = 0 is the heat-sink side; a smaller zMin sits lower. Sort bottom -> top
  // so layer 0 (the Dirichlet boundary side) is the bottom die.
  std::sort(dies.begin(), dies.end(), [](const Die& a, const Die& b) {
    return a.box.zMin() < b.box.zMin();
  });

  // T2 assumes the dies share one in-plane footprint (vertically aligned).
  const odb::Cuboid& ref = dies.front().box;
  for (const Die& d : dies) {
    if (d.box.xMin() != ref.xMin() || d.box.yMin() != ref.yMin()
        || d.box.dx() != ref.dx() || d.box.dy() != ref.dy()) {
      logger->warn(utl::THM,
                   10,
                   "dies do not share a common footprint; T2 uses the bottom "
                   "die footprint and ignores lateral offsets (footprint "
                   "masking is future work)");
      break;
    }
  }

  problem.nx = kGridNx;
  problem.ny = kGridNy;
  problem.dx_m = toMeters(ref.dx()) / problem.nx;
  problem.dy_m = toMeters(ref.dy()) / problem.ny;
  problem.t_bc_celsius = kHeatSinkCelsius;

  // Layers bottom -> top; insert a bond/underfill layer for any vertical gap
  // between consecutive dies (TSV/microbump detail is T4).
  problem.layers.clear();
  is_die_layer.clear();
  long long prev_top_dbu = dies.front().box.zMin();
  for (std::size_t i = 0; i < dies.size(); ++i) {
    const odb::Cuboid& box = dies[i].box;
    if (i > 0) {
      const long long gap_dbu
          = static_cast<long long>(box.zMin()) - prev_top_dbu;
      if (gap_dbu > 0) {
        problem.layers.push_back({toMeters(gap_dbu), kBondWPerMK, kBondVhc});
        is_die_layer.push_back(false);
      } else if (gap_dbu < 0) {
        logger->warn(utl::THM,
                     11,
                     "die '{}' overlaps the die below in z; treating as no gap",
                     dies[i].name);
      }
    }
    problem.layers.push_back({toMeters(box.dz()), kSiliconWPerMK, kSiliconVhc});
    is_die_layer.push_back(true);
    prev_top_dbu = static_cast<long long>(box.zMin()) + box.dz();
  }
  num_dies = static_cast<int>(dies.size());

  // Synthetic power: each die dissipates kPerDieWatts spread uniformly over its
  // cells. Laterally uniform on purpose -> the exact 1-D cross-check applies.
  // Bond layers generate no heat.
  const int nxy = problem.nx * problem.ny;
  const int nz = static_cast<int>(problem.layers.size());
  problem.power_w.assign(static_cast<std::size_t>(nxy) * nz, 0.0);
  for (int l = 0; l < nz; ++l) {
    if (is_die_layer[l]) {
      const double per_cell = kPerDieWatts / nxy;
      for (int c = 0; c < nxy; ++c) {
        problem.power_w[static_cast<std::size_t>(l) * nxy + c] = per_cell;
      }
    }
  }
  return true;
}

// Closed-form peak temperature for a laterally-uniform power distribution: with
// equal power in every cell of a layer and a uniform boundary there is no
// lateral gradient, so the stack reduces exactly to a 1-D series resistor
// ladder. Independent of the FDM solver, so it validates the odb -> problem
// mapping (DBU->SI conversion, z-ordering, footprint area).
double analytic1dPeakCelsius(const core::SteadyProblem& p)
{
  const int nz = static_cast<int>(p.layers.size());
  const int nxy = p.nx * p.ny;
  const double area = static_cast<double>(nxy) * p.dx_m * p.dy_m;

  std::vector<double> layer_power(nz, 0.0);
  double above = 0.0;  // total injected power (crosses the node0 -> bc link)
  for (int l = 0; l < nz; ++l) {
    double s = 0.0;
    for (int c = 0; c < nxy; ++c) {
      s += p.power_w[static_cast<std::size_t>(l) * nxy + c];
    }
    layer_power[l] = s;
    above += s;
  }

  const double r_bc
      = (p.layers[0].thickness_m / (2.0 * p.layers[0].conductivity_w_per_mk))
        / area;
  double t_node = p.t_bc_celsius + above * r_bc;
  double peak = t_node;
  for (int l = 1; l < nz; ++l) {
    above -= layer_power[l - 1];  // power crossing the (l-1, l) link
    const double r_link
        = (p.layers[l - 1].thickness_m
               / (2.0 * p.layers[l - 1].conductivity_w_per_mk)
           + p.layers[l].thickness_m
                 / (2.0 * p.layers[l].conductivity_w_per_mk))
          / area;
    t_node += above * r_link;
    peak = std::max(peak, t_node);
  }
  return peak;
}

// Fallback demo stack (same as T1) when no odb 3D stack is present.
void buildSyntheticStack(core::SteadyProblem& problem)
{
  problem.layers = {
      {300e-6, kSiliconWPerMK, kSiliconVhc},  // logic/base die
      {20e-6, kBondWPerMK, kBondVhc},         // underfill / bond
      {50e-6, kSiliconWPerMK, kSiliconVhc},   // DRAM die
      {20e-6, kBondWPerMK, kBondVhc},         // underfill / bond
      {50e-6, kSiliconWPerMK, kSiliconVhc},   // DRAM die
  };
  problem.nx = 8;
  problem.ny = 8;
  problem.dx_m = 100e-6;
  problem.dy_m = 100e-6;
  problem.t_bc_celsius = kHeatSinkCelsius;
  const int nz = static_cast<int>(problem.layers.size());
  const int nxy = problem.nx * problem.ny;
  problem.power_w.assign(static_cast<std::size_t>(nxy) * nz, 0.0);
  const int top = nz - 1;
  problem.power_w[static_cast<std::size_t>(top) * nxy
                  + (problem.ny / 2) * problem.nx + (problem.nx / 2)]
      = 1.0;
}

// T3 transient demo: backward-Euler step response of `problem` from a uniform
// initial temperature, logging the peak at a few sampled times. The final peak
// should approach the steady peak as total time exceeds the thermal time scale.
void logTransientDemo(utl::Logger* logger,
                      const core::SteadyProblem& problem,
                      double steady_peak_celsius)
{
  constexpr double dt = 5e-6;   // 5 us
  constexpr int steps = 1000;   // -> 5 ms total
  const core::TransientResult tr
      = core::solveTransient(problem, dt, steps, problem.t_bc_celsius);
  const std::vector<double>& h = tr.peak_history_celsius;
  logger->info(utl::THM,
               7,
               "transient: dt = {:.1f} us x {} = {:.2f} ms; peak T(t) {:.3f} -> "
               "{:.3f} -> {:.3f} -> {:.3f} C (steady {:.3f} C)",
               dt * 1e6,
               steps,
               steps * dt * 1e3,
               h.front(),
               h[steps / 10],
               h[steps / 3],
               h.back(),
               steady_peak_celsius);
}

}  // namespace

Thermal::Thermal(odb::dbDatabase* db, utl::Logger* logger)
    : db_(db), logger_(logger)
{
}

void Thermal::analyzeThermal()
{
  odb::dbChip* chip = db_->getChip();
  const char* design = (chip != nullptr) ? "loaded" : "none";
  logger_->info(utl::THM,
                1,
                "thermal module alive (core {}); design: {}",
                core::version(),
                design);

  core::SteadyProblem problem;
  std::vector<bool> is_die_layer;
  int num_dies = 0;
  const bool from_odb
      = (chip != nullptr)
        && buildFromOdbStack(db_, chip, logger_, problem, is_die_layer,
                             num_dies);

  if (from_odb) {
    logger_->info(utl::THM,
                  3,
                  "odb 3D stack: {} dies -> {} layers; grid {}x{}, dx {:.2f} um, "
                  "dy {:.2f} um, T_bc {:.1f} C",
                  num_dies,
                  problem.layers.size(),
                  problem.nx,
                  problem.ny,
                  problem.dx_m * 1e6,
                  problem.dy_m * 1e6,
                  problem.t_bc_celsius);
    for (std::size_t l = 0; l < problem.layers.size(); ++l) {
      logger_->info(utl::THM,
                    4,
                    "  layer {:2d}: {:<4} t = {:7.2f} um, k = {:6.1f} W/m/K",
                    static_cast<int>(l),
                    is_die_layer[l] ? "die" : "bond",
                    problem.layers[l].thickness_m * 1e6,
                    problem.layers[l].conductivity_w_per_mk);
    }
  } else {
    buildSyntheticStack(problem);
  }

  const core::SteadyResult result = core::solveSteady(problem);

  if (from_odb) {
    logger_->info(utl::THM,
                  5,
                  "steady FDM: peak T = {:.3f} C, R_th = {:.4f} K/W",
                  result.peak_celsius,
                  result.r_th_k_per_w);

    // Independent check (see analytic1dPeakCelsius): for this laterally-uniform
    // power the solver must match the 1-D resistor ladder to ~machine precision.
    const double analytic = analytic1dPeakCelsius(problem);
    const double rise = analytic - problem.t_bc_celsius;
    const double rel_err
        = (rise != 0.0)
              ? std::fabs(result.peak_celsius - analytic) / std::fabs(rise)
              : 0.0;
    logger_->info(utl::THM,
                  6,
                  "cross-check vs 1D analytic (uniform power): analytic peak "
                  "{:.4f} C, solver peak {:.4f} C, rel err {:.2e}",
                  analytic,
                  result.peak_celsius,
                  rel_err);
  } else {
    logger_->info(utl::THM,
                  2,
                  "no odb 3D stack; synthetic {}-layer demo: peak T = {:.3f} C, "
                  "R_th = {:.4f} K/W",
                  problem.layers.size(),
                  result.peak_celsius,
                  result.r_th_k_per_w);
  }

  logTransientDemo(logger_, problem, result.peak_celsius);
}

}  // namespace thm

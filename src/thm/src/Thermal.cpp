// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "thm/Thermal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
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

// --- T4a: TSV / microbump effective vertical conductivity (docs/50 §6.1).
// Independently implemented from published mixing rules (parallel rule of
// mixtures for 1-D vertical conduction); no external tool source was used. ---
constexpr double kCuWPerMK = 400.0;           // copper
constexpr double kUnderfillWPerMK = 0.55;     // capillary underfill
constexpr double kSolderWPerMK = 35.0;        // SnAg microbump
constexpr double kDielectricWPerMK = 1.4;     // hybrid-bond dielectric
constexpr double kPi = 3.14159265358979323846;

// Synthetic N-die stack geometry (assumptions; T4 refine).
constexpr double kBaseDieUm = 100.0;          // base/logic die thickness
constexpr double kDramDieUm = 50.0;           // stacked DRAM die thickness
constexpr double kInterDieBondUm = 15.0;      // inter-die bond/underfill gap
constexpr double kStackFootprintUm = 1000.0;  // square die footprint

enum class BondType
{
  kMicrobump,
  kHybrid
};

// Areal fill fraction of a square array: f = (pi/4) (d/p)^2.
double arrayFillFraction(double diameter, double pitch)
{
  const double ratio = diameter / pitch;
  return (kPi / 4.0) * ratio * ratio;
}

// Vertical effective conductivity of a die layer pierced by a Cu TSV array
// (parallel rule of mixtures, exact for 1-D vertical conduction).
double tsvDieKz(double f_tsv)
{
  return f_tsv * kCuWPerMK + (1.0 - f_tsv) * kSiliconWPerMK;
}

// Vertical effective conductivity of the inter-die bond layer:
//   microbump -> SnAg bumps in underfill
//   hybrid    -> continuous Cu pads in dielectric (Cu-Cu direct bond)
double bondKz(BondType type, double fill)
{
  if (type == BondType::kHybrid) {
    return fill * kCuWPerMK + (1.0 - fill) * kDielectricWPerMK;
  }
  return fill * kSolderWPerMK + (1.0 - fill) * kUnderfillWPerMK;
}

struct StackParams
{
  int num_dies;
  BondType bond_type;
  double tsv_diameter_um;
  double tsv_pitch_um;
  double bump_diameter_um;
  double bump_pitch_um;
  double hybrid_cu_coverage;
};

// Build a synthetic N-die HBM-like stack. Heat sink is at the stack bottom
// (core layer 0), so physical die 0 (base/logic) sits on the sink and the
// topmost DRAM die -- farthest from the sink -- runs hottest (Option A; see
// report). die_layer[d] is the core layer index of physical die d (0 = base).
void buildNDieStack(const StackParams& sp,
                    core::SteadyProblem& problem,
                    std::vector<int>& die_layer)
{
  const double f_tsv = arrayFillFraction(sp.tsv_diameter_um, sp.tsv_pitch_um);
  const double f_bond
      = (sp.bond_type == BondType::kHybrid)
            ? sp.hybrid_cu_coverage
            : arrayFillFraction(sp.bump_diameter_um, sp.bump_pitch_um);
  const double die_k = tsvDieKz(f_tsv);
  const double bond_k = bondKz(sp.bond_type, f_bond);

  problem.layers.clear();
  die_layer.assign(sp.num_dies, -1);
  for (int d = 0; d < sp.num_dies; ++d) {
    if (d > 0) {  // bond layer between die d-1 and die d
      problem.layers.push_back({kInterDieBondUm * 1e-6, bond_k, kBondVhc});
    }
    const double thickness = ((d == 0) ? kBaseDieUm : kDramDieUm) * 1e-6;
    die_layer[d] = static_cast<int>(problem.layers.size());
    problem.layers.push_back({thickness, die_k, kSiliconVhc});
  }

  problem.nx = kGridNx;
  problem.ny = kGridNy;
  problem.dx_m = (kStackFootprintUm * 1e-6) / problem.nx;
  problem.dy_m = (kStackFootprintUm * 1e-6) / problem.ny;
  problem.t_bc_celsius = kHeatSinkCelsius;

  // Uniform kPerDieWatts per die spread over its cells; bond layers: no heat.
  const int nxy = problem.nx * problem.ny;
  const int nz = static_cast<int>(problem.layers.size());
  problem.power_w.assign(static_cast<std::size_t>(nxy) * nz, 0.0);
  for (int d = 0; d < sp.num_dies; ++d) {
    const int l = die_layer[d];
    const double per_cell = kPerDieWatts / nxy;
    for (int c = 0; c < nxy; ++c) {
      problem.power_w[static_cast<std::size_t>(l) * nxy + c] = per_cell;
    }
  }
}

// Physical die index (0 = base, on sink) with the highest temperature.
int hottestDie(const core::SteadyResult& result,
               const std::vector<int>& die_layer,
               int nxy,
               double& out_peak_celsius)
{
  int hottest = 0;
  double best = result.temperature_celsius[0];
  for (int d = 0; d < static_cast<int>(die_layer.size()); ++d) {
    const std::size_t base = static_cast<std::size_t>(die_layer[d]) * nxy;
    double layer_peak = result.temperature_celsius[base];
    for (int c = 1; c < nxy; ++c) {
      layer_peak = std::max(layer_peak, result.temperature_celsius[base + c]);
    }
    if (layer_peak > best) {
      best = layer_peak;
      hottest = d;
    }
  }
  out_peak_celsius = best;
  return hottest;
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

void Thermal::sweepThermal(const std::string& dies,
                           double tsv_diameter_um,
                           double tsv_pitch_um,
                           double bump_diameter_um,
                           double bump_pitch_um,
                           double hybrid_cu_coverage)
{
  std::vector<int> stack_sizes;
  {
    std::istringstream iss(dies);
    int n = 0;
    while (iss >> n) {
      if (n > 0) {
        stack_sizes.push_back(n);
      }
    }
  }
  if (stack_sizes.empty()) {
    stack_sizes = {12, 16, 20};
  }

  const double f_tsv = arrayFillFraction(tsv_diameter_um, tsv_pitch_um);
  const double f_bump = arrayFillFraction(bump_diameter_um, bump_pitch_um);
  logger_->info(utl::THM,
                20,
                "N-die sweep (SYNTHETIC stack, heat sink at bottom): TSV d/p "
                "{:.1f}/{:.1f} um (f {:.4f}), microbump d/p {:.1f}/{:.1f} um "
                "(f {:.4f}), hybrid Cu coverage {:.2f}",
                tsv_diameter_um,
                tsv_pitch_um,
                f_tsv,
                bump_diameter_um,
                bump_pitch_um,
                f_bump,
                hybrid_cu_coverage);
  logger_->info(utl::THM,
                21,
                "effective vertical k_z [W/m/K]: die (Si+TSV) {:.2f}, microbump "
                "bond {:.2f}, hybrid bond {:.2f}",
                tsvDieKz(f_tsv),
                bondKz(BondType::kMicrobump, f_bump),
                bondKz(BondType::kHybrid, hybrid_cu_coverage));

  bool monotonic = true;
  double prev_rth = -1.0;
  for (int n : stack_sizes) {
    const StackParams sp{n,
                         BondType::kMicrobump,
                         tsv_diameter_um,
                         tsv_pitch_um,
                         bump_diameter_um,
                         bump_pitch_um,
                         hybrid_cu_coverage};
    core::SteadyProblem problem;
    std::vector<int> die_layer;
    buildNDieStack(sp, problem, die_layer);
    const core::SteadyResult result = core::solveSteady(problem);
    double peak = 0.0;
    const int hot
        = hottestDie(result, die_layer, problem.nx * problem.ny, peak);
    logger_->info(utl::THM,
                  22,
                  "  N {:2d} ({:3d} layers): R_th {:8.4f} K/W, peak T {:8.3f} C, "
                  "hottest = die {} of {} ({})",
                  n,
                  static_cast<int>(problem.layers.size()),
                  result.r_th_k_per_w,
                  peak,
                  hot,
                  n - 1,
                  (hot == n - 1) ? "top, farthest from sink" : "interior");
    if (prev_rth >= 0.0 && result.r_th_k_per_w <= prev_rth) {
      monotonic = false;
    }
    prev_rth = result.r_th_k_per_w;
  }
  logger_->info(utl::THM,
                23,
                "R_th(N) monotonically increasing with stack height: {}",
                monotonic ? "yes" : "NO");

  // Microbump vs hybrid bonding at the largest stack.
  const int n = stack_sizes.back();
  core::SteadyProblem p_bump, p_hyb;
  std::vector<int> dl_bump, dl_hyb;
  buildNDieStack({n,
                  BondType::kMicrobump,
                  tsv_diameter_um,
                  tsv_pitch_um,
                  bump_diameter_um,
                  bump_pitch_um,
                  hybrid_cu_coverage},
                 p_bump,
                 dl_bump);
  buildNDieStack({n,
                  BondType::kHybrid,
                  tsv_diameter_um,
                  tsv_pitch_um,
                  bump_diameter_um,
                  bump_pitch_um,
                  hybrid_cu_coverage},
                 p_hyb,
                 dl_hyb);
  const double rth_bump = core::solveSteady(p_bump).r_th_k_per_w;
  const double rth_hyb = core::solveSteady(p_hyb).r_th_k_per_w;
  const double reduction
      = (rth_bump > 0.0) ? (rth_bump - rth_hyb) / rth_bump * 100.0 : 0.0;
  logger_->info(utl::THM,
                24,
                "bond comparison at N {}: microbump R_th {:.4f}, hybrid R_th "
                "{:.4f} K/W -> hybrid {:.1f}% lower ({})",
                n,
                rth_bump,
                rth_hyb,
                reduction,
                (rth_hyb < rth_bump) ? "hybrid < microbump, expected"
                                     : "UNEXPECTED");
}

void Thermal::dumpThermalStack(int num_dies,
                               const std::string& bond_type,
                               double tsv_diameter_um,
                               double tsv_pitch_um,
                               double bump_diameter_um,
                               double bump_pitch_um,
                               double hybrid_cu_coverage)
{
  const BondType bt = (bond_type == "hybrid") ? BondType::kHybrid
                                              : BondType::kMicrobump;
  const StackParams sp{num_dies,
                       bt,
                       tsv_diameter_um,
                       tsv_pitch_um,
                       bump_diameter_um,
                       bump_pitch_um,
                       hybrid_cu_coverage};
  core::SteadyProblem problem;
  std::vector<int> die_layer;
  buildNDieStack(sp, problem, die_layer);
  const core::SteadyResult result = core::solveSteady(problem);

  const int nz = static_cast<int>(problem.layers.size());
  const int nxy = problem.nx * problem.ny;
  std::vector<bool> is_die(nz, false);
  for (const int l : die_layer) {
    is_die[l] = true;
  }
  double total_power = 0.0;
  for (const double w : problem.power_w) {
    total_power += w;
  }

  // Machine-parseable block (grep DUMP_*). Lengths/areas are SI.
  logger_->info(utl::THM,
                30,
                "DUMP_BEGIN n_dies={} n_layers={} footprint_um={:.4f} nx={} "
                "ny={} t_bc_c={:.4f} total_power_w={:.4f} bond={}",
                num_dies,
                nz,
                problem.dx_m * problem.nx * 1e6,
                problem.nx,
                problem.ny,
                problem.t_bc_celsius,
                total_power,
                (bt == BondType::kHybrid) ? "hybrid" : "microbump");
  for (int l = 0; l < nz; ++l) {
    double layer_power = 0.0;
    double peak = result.temperature_celsius[static_cast<std::size_t>(l) * nxy];
    double sum = 0.0;
    for (int c = 0; c < nxy; ++c) {
      const std::size_t idx = static_cast<std::size_t>(l) * nxy + c;
      peak = std::max(peak, result.temperature_celsius[idx]);
      sum += result.temperature_celsius[idx];
      layer_power += problem.power_w[idx];
    }
    logger_->info(utl::THM,
                  31,
                  "DUMP_LAYER idx={} kind={} thickness_m={:.6e} kz={:.4f} "
                  "vhc={:.4e} power_w={:.4f} peakT_c={:.4f} meanT_c={:.4f}",
                  l,
                  is_die[l] ? "die" : "bond",
                  problem.layers[l].thickness_m,
                  problem.layers[l].conductivity_w_per_mk,
                  problem.layers[l].volumetric_heat_capacity_j_per_m3k,
                  layer_power,
                  peak,
                  sum / nxy);
  }
  logger_->info(utl::THM,
                32,
                "DUMP_RESULT peak_c={:.4f} rth_kw={:.4f}",
                result.peak_celsius,
                result.r_th_k_per_w);
  logger_->info(utl::THM, 33, "DUMP_END");
}

}  // namespace thm
